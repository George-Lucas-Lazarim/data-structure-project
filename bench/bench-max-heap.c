// Rode no terminal: gcc -Wall -Wextra -std=c11 -O2 bench-max-heap.c ../max-heap.c -lm -o bench-max-heap 2>&1 && taskset -c 0 ./bench-max-heap

// https://claude.ai/share/29c9adfa-686b-45bd-8c61-56895f59f92f Conversa com o Claude
#define _POSIX_C_SOURCE 200809L // 2008.09

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <math.h>

#include "../max-heap.h"

// Parâmetros do experimento
#define N_MAX 60000 // Teto seguro: capacity e size sao uint16_t (maximo 65535)
#define REPS  9      // Repetições por medida (para tirar a mediana)

// Semente fixa: as severidades sorteadas (que definem a ordem na heap) ficam reprodutiveis
// entre execucoes. Pode trocar com -DBENCH_SEED=...
#ifndef BENCH_SEED
#define BENCH_SEED 20260625ULL
#endif

static volatile uint64_t g_sink = 0; // Consome os dados para o compilador não apagar os laços

// ---------------------------------------------------------------------------
// PRNG proprio do benchmark (splitmix64), INDEPENDENTE do rand() da libc.
// A max-heap nao usa rand(); manter o sorteio das severidades num gerador a parte e com
// semente fixa garante datasets reprodutiveis e iguais entre execucoes do benchmark.
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

// Mede insercao (sift-up) e extracao do maximo (sift-down) para um dado n (medianas, ns/op).
// A heap guarda PONTEIROS para alertas que vivem no vetor externo 'alerts'.
static void benchHeap (DTCAlert* alerts, int n, double* ins, double* ext) {
    MaxHeap mh;
    long long it[REPS], et[REPS];
    struct timespec a, b;

    // Tempo de inserção: Constroi do zero a cada repetição (n insercoes, cada uma O(log n) no
    // pior caso; em media, com chaves aleatorias, o sift-up sobe poucos niveis -> ~O(1) amortizado)
    for (int j = 0; j < REPS; j++) {
        initMaxHeap(&mh, (uint16_t) n);
        clock_gettime(CLOCK_MONOTONIC, &a);
        for (int i = 0; i < n; i++) insertMaxHeap(&mh, &alerts[i]);
        clock_gettime(CLOCK_MONOTONIC, &b);
        it[j] = elapsed_ns(a, b);
        freeMaxHeap(&mh);
    }
    *ins = (double) median(it, REPS) / n;

    // Tempo de extração: Constroi do zero a cada repetição e drena todos os n maximos.
    // Cada extractMax leva a ultima folha ao topo e a faz descer (sift-down), tipicamente
    // O(log n) -> drenar tudo e O(n log n). Mede a remocao na unica forma que a heap oferece.
    for (int j = 0; j < REPS; j++) {
        initMaxHeap(&mh, (uint16_t) n);
        for (int i = 0; i < n; i++) insertMaxHeap(&mh, &alerts[i]);
        clock_gettime(CLOCK_MONOTONIC, &a);
        for (int i = 0; i < n; i++) {
            DTCAlert* top = extractMax(&mh);
            g_sink += top->severity;
        }
        clock_gettime(CLOCK_MONOTONIC, &b);
        et[j] = elapsed_ns(a, b);
        freeMaxHeap(&mh);
    }
    *ext = (double) median(et, REPS) / n;
}

int main (void) {
    benchSrand(BENCH_SEED);

    // Pré-gera N_MAX alertas com severidade aleatoria. A severidade (uint8_t, 0..255) e a chave
    // de ordenacao da heap; usar toda a faixa [0,255] maximiza prioridades distintas e exercita
    // os caminhos de sift (uma distribuicao real, estreita e enviesada para baixas severidades,
    // teria mais empates e sifts ainda mais curtos). Gerado fora da janela cronometrada -> a
    // sintese dos dados nao entra na conta.
    DTCAlert* alerts = (DTCAlert*) malloc ((size_t) N_MAX * sizeof(DTCAlert));
    if (alerts == NULL) {
        fprintf(stderr, "malloc falhou\n");
        return 1;
    }
    for (int i = 0; i < N_MAX; i++) {
        alerts[i].raw_code = (uint16_t) i;
        alerts[i].severity = (uint8_t) benchRandBelow(256); // 0..255
        alerts[i].obd2_code[0] = '\0';
        alerts[i].description[0] = '\0';
    }

    // Passos geometricos: como insert/extract sao O(log n), passos constantes mostrariam
    // diferencas minimas. Espalhar n por quase duas ordens de grandeza revela a curva log.
    int ns[] = { 1000, 3000, 10000, 30000, 60000 };
    int n_count = (int)(sizeof(ns) / sizeof(ns[0]));

    FILE* csv = fopen("bench_heap.csv", "w");
    if (csv == NULL) {
        fprintf(stderr, "fopen falhou\n");
        return 1;
    }
    fprintf(csv, "n,insert_ns_op,extract_ns_op,log2_n,mem_bytes\n");

    size_t ptr = sizeof(DTCAlert*);
    printf("\nsizeof(DTCAlert*)=%zu B (a heap guarda ponteiros; os alertas vivem em vetor externo)\n", ptr);
    printf("%8s %12s %12s %9s %12s\n", "n", "insert(ns)", "extract(ns)", "log2(n)", "mem(bytes)");

    for (int t = 0; t < n_count; t++) {
        int n = ns[t];

        double ins, ext;
        benchHeap(alerts, n, &ins, &ext);

        double log2_n = log2((double) n); // referencia teorica O(log n)
        long long mem = (long long) n * (long long) ptr + (long long) sizeof(MaxHeap);

        printf("%8d %12.3f %12.3f %9.2f %12lld\n", n, ins, ext, log2_n, mem);
        fprintf(csv, "%d,%.3f,%.3f,%.2f,%lld\n", n, ins, ext, log2_n, mem);
    }

    fclose(csv);
    free(alerts);
    return 0;
}
