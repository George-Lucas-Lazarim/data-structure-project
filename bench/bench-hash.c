// Rode no terminal: gcc -Wall -Wextra -std=c11 -O2 bench-hash.c ../hash.c -lm -o bench-hash 2>&1 && taskset -c 0 ./bench-hash

// https://claude.ai/share/29c9adfa-686b-45bd-8c61-56895f59f92f Conversa com o Claude
#define _POSIX_C_SOURCE 200809L // 2008.09

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include "../hash.h"

// Parâmetros do experimento
#define N_MAX    60000   // Teto seguro: total_elements e size sao uint16_t (maximo 65535)
#define REPS     11      // Repetições por medida (para tirar a mediana)
#define SEARCHES 2000    // Buscas por medida de tempo de busca

// Semente fixa: a permutacao de chaves, os indices de busca e a ordem de remocao ficam
// IDENTICOS entre os tres tamanhos de tabela. Assim o unico fator que muda entre as
// configuracoes e o tamanho (logo, o fator de carga). Pode trocar com -DBENCH_SEED=...
#ifndef BENCH_SEED
#define BENCH_SEED 20260625ULL
#endif

static volatile uint64_t g_sink = 0; // Consome os dados para o compilador não apagar os laços

// ---------------------------------------------------------------------------
// PRNG proprio do benchmark (splitmix64), INDEPENDENTE do rand() da libc.
// A hash em si nao usa rand(); manter as escolhas do benchmark (a permutacao de
// raw_codes, quais chaves buscar e em que ordem remover) num gerador a parte e com
// semente fixa garante que as TRES configuracoes de tamanho recebam exatamente as
// mesmas chaves e a mesma sequencia de operacoes, isolando o fator de carga.
// ---------------------------------------------------------------------------
static uint64_t bench_rng_state = 0;

static void benchSrand (uint64_t seed) {
    bench_rng_state = seed;
}

static uint32_t benchRandBelow (uint32_t bound) {
    uint64_t z = (bench_rng_state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    z = z ^ (z >> 31);
    return (uint32_t)(z % bound); // vies de modulo desprezivel para os tamanhos aqui
}

static long long elapsed_ns (struct timespec a, struct timespec b) {
    return (long long)(b.tv_sec - a.tv_sec) * 1000000000LL + (b.tv_nsec - a.tv_nsec);
}

static int cmp_ll (const void* x, const void* y) {
    long long a = *(const long long*) x, b = *(const long long*) y;
    return (a > b) - (a < b);
}

static long long median (long long* v, int n) {
    qsort(v, n, sizeof(long long), cmp_ll);
    return v[n / 2];
}

// Mede insercao, busca (bem-sucedida) e remocao de chave arbitraria para um dado tamanho
// de tabela (medianas, em ns por operacao). Tambem coleta colisoes e maior cadeia.
static void benchHash (uint16_t size, const DTCAlert* alerts, const int* idx, const int* perm,
                       int n, double* ins, double* sea, double* rem,
                       uint16_t* collisions, uint8_t* longest, double* load) {
    DTCHashTable h;
    long long it[REPS], st[REPS], dt[REPS]; // Vetores que guardam os tempos de cada operação
    struct timespec a, b;

    // Tempo de inserção: Constroi do zero a cada repetição
    for (int j = 0; j < REPS; j++) {
        initDTCHashTable(&h, size);
        clock_gettime(CLOCK_MONOTONIC, &a);
        for (int i = 0; i < n; i++) insertDTC(&h, alerts[i]);
        clock_gettime(CLOCK_MONOTONIC, &b);
        it[j] = elapsed_ns(a, b);
        freeDTCHashTable(&h);
    }
    *ins = (double) median(it, REPS) / n;

    // Constroi uma vez para medir busca, colisoes e maior cadeia
    initDTCHashTable(&h, size);
    for (int i = 0; i < n; i++) insertDTC(&h, alerts[i]);

    // warm-up
    for (int k = 0; k < SEARCHES; k++) {
        DTCAlert* r = searchDTC(&h, alerts[idx[k]].raw_code);
        if (r != NULL) g_sink += r->severity;
    }

    // Busca em lote: cronometra as SEARCHES de uma vez (elimina o overhead do clock por
    // chamada) e divide por SEARCHES; a mediana entre as REPS protege contra uma repeticao
    // contaminada por preempcao. So buscamos chaves existentes (busca com sucesso).
    for (int j = 0; j < REPS; j++) {
        clock_gettime(CLOCK_MONOTONIC, &a);
        for (int k = 0; k < SEARCHES; k++) {
            DTCAlert* r = searchDTC(&h, alerts[idx[k]].raw_code);
            if (r != NULL) g_sink += r->severity;
        }
        clock_gettime(CLOCK_MONOTONIC, &b);
        st[j] = elapsed_ns(a, b);
    }
    *sea = (double) median(st, REPS) / SEARCHES;

    // Estatisticas de colisao (com a tabela ainda construida)
    *collisions = getTotalCollisions(&h);
    *longest    = getLongestChain(&h);
    *load       = (double) n / size;

    freeDTCHashTable(&h);

    // Tempo de remoção: Constroi do zero a cada repetição e remove em ordem embaralhada.
    // Remover em ordem aleatoria (perm) mede a remocao de um elemento QUALQUER (hash +
    // varredura da cadeia + religar ponteiros), e nao um padrao artificialmente favoravel.
    for (int j = 0; j < REPS; j++) {
        initDTCHashTable(&h, size);
        for (int i = 0; i < n; i++) insertDTC(&h, alerts[i]);
        clock_gettime(CLOCK_MONOTONIC, &a);
        for (int i = 0; i < n; i++) removeDTC(&h, alerts[perm[i]].raw_code);
        clock_gettime(CLOCK_MONOTONIC, &b);
        dt[j] = elapsed_ns(a, b);
        freeDTCHashTable(&h);
    }
    *rem = (double) median(dt, REPS) / n;
}

int main (void) {
    benchSrand(BENCH_SEED);

    // Pré-gera N_MAX alertas com raw_code UNICO (permutacao de [0, 65536) por Fisher-Yates).
    // Chaves unicas isolam "colisao de hash" de "chave duplicada": como insertDTC nao checa
    // duplicatas, repetir um raw_code criaria cadeia sem relacao com o fator de carga.
    // Tudo gerado fora da janela cronometrada -> a sintese dos dados nao entra na conta.
    uint16_t* keyspace = (uint16_t*) malloc (65536 * sizeof(uint16_t));
    DTCAlert* alerts   = (DTCAlert*) malloc ((size_t) N_MAX * sizeof(DTCAlert));
    int* idx  = (int*) malloc ((size_t) SEARCHES * sizeof(int)); // Indices de busca
    int* perm = (int*) malloc ((size_t) N_MAX * sizeof(int));    // Ordem de remocao
    if (keyspace == NULL || alerts == NULL || idx == NULL || perm == NULL) {
        fprintf(stderr, "malloc falhou\n");
        return 1;
    }

    for (int i = 0; i < 65536; i++) keyspace[i] = (uint16_t) i;
    for (int i = 65535; i > 0; i--) { // Fisher-Yates no espaco de chaves
        int jdx = (int) benchRandBelow((uint32_t) (i + 1));
        uint16_t tmp = keyspace[i]; keyspace[i] = keyspace[jdx]; keyspace[jdx] = tmp;
    }

    for (int i = 0; i < N_MAX; i++) {
        alerts[i].raw_code = keyspace[i];                 // chave unica (so isto importa p/ hash)
        alerts[i].severity = (uint8_t) benchRandBelow(6); // 0..5 (preenchido por realismo)
        snprintf(alerts[i].obd2_code, sizeof(alerts[i].obd2_code), "P%04X", keyspace[i] & 0xFFF);
        alerts[i].description[0] = '\0';
    }

    // Tres tamanhos de tabela PRIMOS fixos. Como size e fixo, o fator de carga (alpha = n/size)
    // cresce com n: a tabela pequena satura (alpha alto, cadeias longas, busca O(alpha)); a
    // grande mantem alpha baixo (busca ~O(1)). O tamanho primo aproxima o hashing por modulo
    // de uma distribuicao uniforme e evita artefatos de divisibilidade com a permutacao.
    uint16_t sizes[] = { 4099, 16411, 65521 };
    int size_count = (int)(sizeof(sizes) / sizeof(sizes[0]));

    int ns[] = { 1000, 2000, 5000, 10000, 20000, 40000, 60000 };
    int n_count = (int)(sizeof(ns) / sizeof(ns[0]));

    FILE* csv = fopen("bench_hash.csv", "w");
    if (csv == NULL) {
        fprintf(stderr, "fopen falhou\n");
        return 1;
    }
    fprintf(csv, "table_size,n,load_factor,insert_ns_op,search_ns_op,remove_ns_op,collisions,longest_chain,mem_bytes\n");

    size_t node = sizeof(HashNode);
    printf("\nsizeof(DTCAlert)=%zu B | sizeof(HashNode)=%zu B\n", sizeof(DTCAlert), node);

    for (int s = 0; s < size_count; s++) {
        uint16_t size = sizes[s];

        printf("\nTabela Hash | size = %u (primo)\n", size);
        printf("%8s %8s %12s %12s %12s %11s %8s %12s\n",
               "n", "alpha", "insert(ns)", "search(ns)", "remove(ns)", "colisoes", "maior", "mem(bytes)");

        for (int t = 0; t < n_count; t++) {
            int n = ns[t];

            // Chaves de busca e ordem de remocao: sorteadas pelo PRNG do benchmark em [0, n)
            for (int k = 0; k < SEARCHES; k++) idx[k] = (int) benchRandBelow((uint32_t) n);
            for (int i = 0; i < n; i++) perm[i] = i;
            for (int i = n - 1; i > 0; i--) {
                int jdx = (int) benchRandBelow((uint32_t) (i + 1));
                int tmp = perm[i]; perm[i] = perm[jdx]; perm[jdx] = tmp;
            }

            double ins, sea, rem, load;
            uint16_t collisions;
            uint8_t longest;
            benchHash(size, alerts, idx, perm, n, &ins, &sea, &rem, &collisions, &longest, &load);

            long long mem = (long long) size * (long long) sizeof(HashNode*)
                          + (long long) n * (long long) node;

            printf("%8d %8.3f %12.3f %12.3f %12.3f %11u %8u %12lld\n",
                   n, load, ins, sea, rem, collisions, longest, mem);

            fprintf(csv, "%u,%d,%.4f,%.3f,%.3f,%.3f,%u,%u,%lld\n",
                    size, n, load, ins, sea, rem, collisions, longest, mem);
        }
    }

    fclose(csv);
    free(perm);
    free(idx);
    free(alerts);
    free(keyspace);
    return 0;
}
