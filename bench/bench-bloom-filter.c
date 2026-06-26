// Rode no terminal: gcc -Wall -Wextra -std=c11 -O2 bench-bloom-filter.c ../bloom-filter.c -lm -o bench-bloom 2>&1 && taskset -c 0 ./bench-bloom

// https://claude.ai/share/29c9adfa-686b-45bd-8c61-56895f59f92f Conversa com o Claude
#define _POSIX_C_SOURCE 200809L // 2008.09

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <math.h>

#include "../bloom-filter.h"

// Parâmetros do experimento
#define M_BITS  65521   // Tamanho do filtro em bits (primo, <= 65535 pois size_in_bits e uint16_t)
#define N_MAX   20000   // Maximo de chaves inseridas
#define QUERIES 40000   // Consultas de NAO-membros para estimar o FPR (N_MAX + QUERIES < 65536)
#define REPS    9       // Repetições por medida de TEMPO (para tirar a mediana)

// Semente fixa: a permutacao de chaves e os conjuntos de insercao/consulta ficam IDENTICOS
// entre os valores de k. Assim o unico fator que muda e k. Pode trocar com -DBENCH_SEED=...
#ifndef BENCH_SEED
#define BENCH_SEED 20260625ULL
#endif

static volatile uint64_t g_sink = 0; // Consome os dados para o compilador não apagar os laços

// ---------------------------------------------------------------------------
// PRNG proprio do benchmark (splitmix64), INDEPENDENTE do rand() da libc.
// O bloom filter nao usa rand(); manter a permutacao de raw_codes num gerador a parte e com
// semente fixa garante que todos os valores de k recebam exatamente as mesmas chaves inseridas
// e as mesmas chaves consultadas, isolando o efeito do numero de funcoes de hash.
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

// FPR teorico esperado: (1 - e^(-k*n/m))^k. Modelo classico assumindo k hashes independentes
// e uniformes sobre m bits apos n insercoes.
static double theoreticalFPR (int k, int n, int m) {
    double exponent = -(double) k * (double) n / (double) m;
    double p_bit_set = 1.0 - exp(exponent); // probabilidade de um bit especifico estar em 1
    return pow(p_bit_set, k);
}

int main (void) {
    benchSrand(BENCH_SEED);

    // Pré-gera as chaves: permutacao de [0, 65536) por Fisher-Yates.
    // Os N_MAX primeiros sao candidatos a INSERCAO (prefixo [0, n) muda com n);
    // a faixa [N_MAX, N_MAX + QUERIES) sao NAO-membros (nunca inseridos, pois a insercao e
    // sempre um prefixo <= N_MAX) -> toda consulta a essa faixa que retorne 'true' e um falso
    // positivo. Tudo gerado fora da janela cronometrada -> a sintese nao entra na conta.
    uint16_t* keys = (uint16_t*) malloc (65536 * sizeof(uint16_t));
    if (keys == NULL) {
        fprintf(stderr, "malloc falhou\n");
        return 1;
    }
    for (int i = 0; i < 65536; i++) keys[i] = (uint16_t) i;
    for (int i = 65535; i > 0; i--) {
        int jdx = (int) benchRandBelow((uint32_t) (i + 1));
        uint16_t tmp = keys[i]; keys[i] = keys[jdx]; keys[jdx] = tmp;
    }

    int k_values[] = { 4, 6, 8 };
    int k_count = (int)(sizeof(k_values) / sizeof(k_values[0]));

    int ns[] = { 1000, 2000, 5000, 10000, 15000, 20000 };
    int n_count = (int)(sizeof(ns) / sizeof(ns[0]));

    FILE* csv = fopen("bench_bloom.csv", "w");
    if (csv == NULL) {
        fprintf(stderr, "fopen falhou\n");
        return 1;
    }
    fprintf(csv, "k,m_bits,n,insert_ns_op,check_hit_ns_op,check_miss_ns_op,occupancy,fpr_empirical,fpr_estimated,fpr_theoretical,mem_bytes\n");

    long long mem = (long long)((M_BITS + 7) / 8); // bytes do bit_array

    for (int kk = 0; kk < k_count; kk++) {
        int k = k_values[kk];

        printf("\nBloom Filter | m = %d bits | k = %d | k otimo ~ (m/n)*ln2\n", M_BITS, k);
        printf("%8s %12s %12s %12s %11s %13s %13s %13s\n",
               "n", "insert(ns)", "hit(ns)", "miss(ns)", "ocupacao", "fpr_medido", "fpr_estim", "fpr_teorico");

        for (int t = 0; t < n_count; t++) {
            int n = ns[t];
            BloomFilter bf;
            initBloomFilter(&bf, (uint16_t) M_BITS, (uint8_t) k);
            struct timespec a, b;

            // Tempo de inserção: limpa (fora da janela) e insere n chaves a cada repetição
            long long it[REPS];
            for (int j = 0; j < REPS; j++) {
                clearBloomFilter(&bf);
                clock_gettime(CLOCK_MONOTONIC, &a);
                for (int i = 0; i < n; i++) insertBloomFilter(&bf, keys[i]);
                clock_gettime(CLOCK_MONOTONIC, &b);
                it[j] = elapsed_ns(a, b);
            }
            double insert_ns_op = (double) median(it, REPS) / n;
            // Ao fim do laco o filtro contem exatamente keys[0..n) (insercoes da ultima repeticao)

            // FPR empirico (deterministico dadas as chaves): consulta os QUERIES nao-membros
            int false_positives = 0;
            for (int q = 0; q < QUERIES; q++)
                if (checkBloomFilter(&bf, keys[N_MAX + q])) false_positives++;
            double fpr_emp = (double) false_positives / QUERIES;

            // Tempo de consulta com ACERTO: chaves existentes (sempre retornam true; o bloom nao
            // tem falso negativo, entao percorre todos os k bits)
            long long ht[REPS];
            for (int j = 0; j < REPS; j++) {
                clock_gettime(CLOCK_MONOTONIC, &a);
                for (int i = 0; i < n; i++)
                    if (checkBloomFilter(&bf, keys[i])) g_sink++;
                clock_gettime(CLOCK_MONOTONIC, &b);
                ht[j] = elapsed_ns(a, b);
            }
            double hit_ns_op = (double) median(ht, REPS) / n;

            // Tempo de consulta com ERRO: nao-membros (a maioria sai cedo no primeiro bit 0;
            // por isso miss tende a ser mais barato que hit)
            long long mt[REPS];
            for (int j = 0; j < REPS; j++) {
                clock_gettime(CLOCK_MONOTONIC, &a);
                for (int q = 0; q < QUERIES; q++)
                    if (checkBloomFilter(&bf, keys[N_MAX + q])) g_sink++;
                clock_gettime(CLOCK_MONOTONIC, &b);
                mt[j] = elapsed_ns(a, b);
            }
            double miss_ns_op = (double) median(mt, REPS) / QUERIES;

            double occ      = getBloomFilterOccupancy(&bf);   // fracao de bits em 1
            double fpr_est  = estimateFalsePositiveRate(&bf); // ocupacao^k (estimativa a posteriori)
            double fpr_theo = theoreticalFPR(k, n, M_BITS);   // (1 - e^(-kn/m))^k

            freeBloomFilter(&bf);

            printf("%8d %12.3f %12.3f %12.3f %11.4f %13.6f %13.6f %13.6f\n",
                   n, insert_ns_op, hit_ns_op, miss_ns_op, occ, fpr_emp, fpr_est, fpr_theo);

            fprintf(csv, "%d,%d,%d,%.3f,%.3f,%.3f,%.4f,%.6f,%.6f,%.6f,%lld\n",
                    k, M_BITS, n, insert_ns_op, hit_ns_op, miss_ns_op, occ, fpr_emp, fpr_est, fpr_theo, mem);
        }
    }

    fclose(csv);
    free(keys);
    return 0;
}
