// Rode no terminal (apague o csv antes para nao acumular linhas de execucoes antigas):
// gcc -Wall -Wextra -std=c11 -O2 bench-skip-list.c ../synthetic-data.c ../skip-list.c -lm -o bench_sl_p50
// gcc -Wall -Wextra -std=c11 -O2 -DSKIPLIST_P_THRESHOLD="(RAND_MAX/4)" bench-skip-list.c ../synthetic-data.c ../skip-list.c -lm -o bench_sl_p25
// rm -f bench_skiplist.csv && taskset -c 0 ./bench_sl_p50 bench_skiplist.csv && taskset -c 0 ./bench_sl_p25 bench_skiplist.csv

// https://claude.ai/share/97e8f61d-b35f-4ee4-9bdb-612d05ef56fc Conversa com o Claude
#define _POSIX_C_SOURCE 200809L // 2008.09

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include "../synthetic-data.h"
#include "../skip-list.h"

// Parâmetros do experimento
#define N_MAX    100000
#define REPS     9       // Repetições por medida (para tirar a mediana)
#define SEARCHES 2000    // Buscas por medida de tempo de busca

// Semente fixa: dataset, indices de busca e ordem de remocao ficam IDENTICOS entre os dois
// executaveis (p=0.5 e p=0.25). Assim o unico fator que muda entre as duas execucoes e o p.
// Pode trocar com -DBENCH_SEED=...
#ifndef BENCH_SEED
#define BENCH_SEED 20260625ULL
#endif

static volatile uint64_t g_sink = 0; // Consome os dados para o compilador não apagar os laços

// ---------------------------------------------------------------------------
// PRNG proprio do benchmark (splitmix64), INDEPENDENTE do rand() da libc.
// O rand() e usado pela geracao do dataset (deterministica com srand fixo) e pelo
// randomLevel() da skip list (e ai que o p atua, divergindo entre as duas execucoes).
// Manter as escolhas do benchmark (quais chaves buscar, em que ordem remover) num
// gerador a parte garante que elas sejam as mesmas nos dois p, sem serem afetadas
// pelo numero de sorteios que o randomLevel() consome.
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

// Caminha pelo nivel 0 (todos os nos) e soma as alturas -> memoria de ponteiros e histograma
static void walkStats (const SkipList* sl, long long* total_levels, int* hist) {
    long long t = 0;
    for (int i = 0; i <= SKIPLIST_MAX_LEVEL; i++) hist[i] = 0;
    SkipListNode* cur = sl->header->forward[0];
    while (cur != NULL) {
        t += cur->level;
        if (cur->level <= SKIPLIST_MAX_LEVEL) hist[cur->level]++;
        cur = cur->forward[0];
    }
    *total_levels = t;
}

int main (int argc, char** argv) {
    const char* csv_path = (argc > 1) ? argv[1] : "bench_skiplist.csv";

    initEngineSimulation();
    srand(BENCH_SEED);          // dataset reprodutivel e identico entre os dois executaveis
    benchSrand(BENCH_SEED);     // mesmas chaves de busca e mesma ordem de remocao nos dois p

    // p efetivo a partir do limiar compilado
    double p_eff = (double) SKIPLIST_P_THRESHOLD / ((double) RAND_MAX + 1.0);

    // Pré-gera N_MAX amostras uma unica vez (fora da medição)
    EngineSensorsData* data = (EngineSensorsData*) malloc ((size_t) N_MAX * sizeof(EngineSensorsData));
    if (data == NULL) {
        fprintf(stderr, "malloc falhou\n");
        return 1;
    }
    for (int i = 0; i < N_MAX; i++) data[i] = getNextSensorsDataBlock();

    int* idx  = (int*) malloc ((size_t) SEARCHES * sizeof(int)); // Indices de busca (pré-gerados)
    int* perm = (int*) malloc ((size_t) N_MAX * sizeof(int));    // Ordem de remocao embaralhada
    if (idx == NULL || perm == NULL) {
        fprintf(stderr, "malloc falhou\n");
        return 1;
    }

    FILE* csv = fopen(csv_path, "a");
    if (csv == NULL) {
        fprintf(stderr, "fopen falhou\n");
        return 1;
    }
    // Escreve o cabecalho apenas se o arquivo estiver vazio (primeiro executavel a abrir)
    fseek(csv, 0, SEEK_END);
    if (ftell(csv) == 0)
        fprintf(csv, "p,n,insert_ns_op,search_ns_op,remove_ns_op,mem_bytes,avg_height,max_level\n");

    int ns[] = { 1000, 3000, 10000, 30000, 100000 };
    int n_count = (int)(sizeof(ns) / sizeof(ns[0]));

    printf("\nSkip List | p = %.3f\n", p_eff);
    printf("%8s %12s %12s %12s %12s %11s %9s\n", "n", "insert(ns)", "search(ns)", "remove(ns)", "mem(bytes)", "alt.media", "niveis");

    for (int t = 0; t < n_count; t++) {
        int n = ns[t];
        SkipList sl;
        struct timespec a, b;

        // Tempo de inserção: Constroi do zero a cada repetição
        long long it[REPS];
        for (int j = 0; j < REPS; j++) {
            initSkipList(&sl);
            clock_gettime(CLOCK_MONOTONIC, &a);
            for (int i = 0; i < n; i++) insertSkipList(&sl, data[i]);
            clock_gettime(CLOCK_MONOTONIC, &b);
            it[j] = elapsed_ns(a, b);
            freeSkipList(&sl);
        }
        double insert_ns_op = (double) median(it, REPS) / n;

        // Constroi uma vez para medir busca, memoria e histograma
        initSkipList(&sl);
        for (int i = 0; i < n; i++) insertSkipList(&sl, data[i]);

        // Chaves de busca: SEARCHES indices em [0, n) sorteados pelo PRNG do benchmark
        for (int k = 0; k < SEARCHES; k++) idx[k] = (int) benchRandBelow((uint32_t) n);

        EngineSensorsData out;

        // warm-up
        for (int k = 0; k < SEARCHES; k++)
            if (searchSkipList(&sl, data[idx[k]].time_ms, &out)) g_sink += out.rpm;

        // Busca em lote: cronometra as SEARCHES de uma vez (elimina o overhead do clock por
        // chamada) e divide por SEARCHES; a mediana entre as REPS protege contra uma
        // repeticao contaminada por preempcao.
        long long st[REPS];
        for (int j = 0; j < REPS; j++) {
            clock_gettime(CLOCK_MONOTONIC, &a);
            for (int k = 0; k < SEARCHES; k++)
                if (searchSkipList(&sl, data[idx[k]].time_ms, &out)) g_sink += out.rpm;
            clock_gettime(CLOCK_MONOTONIC, &b);
            st[j] = elapsed_ns(a, b);
        }
        double search_ns_op = (double) median(st, REPS) / SEARCHES;

        // Estatisticas de memória e altura (com a lista ainda construida)
        long long total_levels;
        int hist[SKIPLIST_MAX_LEVEL + 1];
        walkStats(&sl, &total_levels, hist);

        long long mem = (long long) n * (long long) sizeof(SkipListNode)
                      + total_levels * (long long) sizeof(SkipListNode*)
                      + (long long) sizeof(SkipList)
                      + (long long) SKIPLIST_MAX_LEVEL * (long long) sizeof(SkipListNode*);
        double avg_height = (double) total_levels / n;
        int max_level = sl.level;

        freeSkipList(&sl);

        // Ordem de remocao embaralhada (Fisher-Yates), fora da janela cronometrada.
        // Mede remocao de um elemento QUALQUER (busca O(log n) + religar ponteiros),
        // e nao a drenagem barata pela frente da lista.
        for (int i = 0; i < n; i++) perm[i] = i;
        for (int i = n - 1; i > 0; i--) {
            int jdx = (int) benchRandBelow((uint32_t) (i + 1));
            int tmp = perm[i]; perm[i] = perm[jdx]; perm[jdx] = tmp;
        }

        // Tempo de remoção: Constroi do zero a cada repetição e remove em ordem embaralhada
        long long dt[REPS];
        for (int j = 0; j < REPS; j++) {
            initSkipList(&sl);
            for (int i = 0; i < n; i++) insertSkipList(&sl, data[i]);
            clock_gettime(CLOCK_MONOTONIC, &a);
            for (int i = 0; i < n; i++) removeSkipList(&sl, data[perm[i]].time_ms);
            clock_gettime(CLOCK_MONOTONIC, &b);
            dt[j] = elapsed_ns(a, b);
            freeSkipList(&sl);
        }
        double remove_ns_op = (double) median(dt, REPS) / n;

        printf("%8d %12.2f %12.2f %12.2f %12lld %11.3f %9d\n",
               n, insert_ns_op, search_ns_op, remove_ns_op, mem, avg_height, max_level);

        fprintf(csv, "%.2f,%d,%.3f,%.3f,%.3f,%lld,%.4f,%d\n",
                p_eff, n, insert_ns_op, search_ns_op, remove_ns_op, mem, avg_height, max_level);

        if (n == N_MAX) {
            printf("Histograma de niveis (n = %d): ", n);
            for (int L = 1; L <= max_level; L++) printf("L%d=%d ", L, hist[L]);
            printf("\n");
        }
    }

    fclose(csv);
    free(perm);
    free(idx);
    free(data);
    return 0;
}