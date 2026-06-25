// Rode no terminal: gcc -Wall -Wextra -std=c11 -O2 bench-queue.c ../synthetic-data.c ../queue.c -lm -o bench-queue 2>&1 && taskset -c 0 ./bench-queue

// https://claude.ai/share/a86398db-6bfc-4668-998d-54a74debbf10 Conversa com o Claude
#define _POSIX_C_SOURCE 200809L // 2008.09

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include "../synthetic-data.h"
#include "../queue.h"

// Parâmetros do experimento
#define N_MIN  1000
#define N_MAX  10000
#define N_STEP 1000
#define REPS   51 // Repetições por medida (para tirar a mediana)

static volatile uint64_t g_sink = 0; // Consome os dados para o compilador não apagar os laços

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

// Fila estática: mede enqueue, dequeue e ciclo combinado (medianas, em ns totais)
static void benchStatic (const EngineSensorsData* data, int n,
                         long long* enq, long long* deq, long long* cyc) {
    static StaticCircularQueue q; // 200 KB: armazenamento estatico, reutilizado
    EngineSensorsData aux;
    long long et[REPS], dt[REPS], ct[REPS]; // Vetores que guardam os 51 tempos de cada operação
    struct timespec a, b;

    // warm-up
    initStaticCircularQueue(&q);
    for (int i = 0; i < n; i++) enqueueStaticCircularQueue(&q, data[i]);
    for (int i = 0; i < n; i++) {
        dequeueStaticCircularQueue(&q, &aux); 
        g_sink += aux.rpm;
    }

    for (int j = 0; j < REPS; j++) {
        initStaticCircularQueue(&q);
        clock_gettime(CLOCK_MONOTONIC, &a);
        for (int i = 0; i < n; i++) enqueueStaticCircularQueue(&q, data[i]);
        clock_gettime(CLOCK_MONOTONIC, &b);
        et[j] = elapsed_ns(a, b);

        clock_gettime(CLOCK_MONOTONIC, &a);
        for (int i = 0; i < n; i++) {
            dequeueStaticCircularQueue(&q, &aux);
            g_sink += aux.rpm;
        }
        clock_gettime(CLOCK_MONOTONIC, &b);
        dt[j] = elapsed_ns(a, b);

        initStaticCircularQueue(&q);
        clock_gettime(CLOCK_MONOTONIC, &a);
        for (int i = 0; i < n; i++) {
            enqueueStaticCircularQueue(&q, data[i]);
            dequeueStaticCircularQueue(&q, &aux);
            g_sink += aux.rpm;
        }
        clock_gettime(CLOCK_MONOTONIC, &b);
        ct[j] = elapsed_ns(a, b);
    }

    // Retorno da função
    *enq = median(et, REPS);
    *deq = median(dt, REPS);
    *cyc = median(ct, REPS);
}

// Fila dinamica: idem a fila circular estática
static void benchDynamic (const EngineSensorsData* data, int n,
                          long long* enq, long long* deq, long long* cyc) {
    DynamicQueue q;
    EngineSensorsData aux;
    long long et[REPS], dt[REPS], ct[REPS];
    struct timespec a, b;

    initDynamicQueue(&q);
    for (int i = 0; i < n; i++) enqueueDynamicQueue(&q, data[i]);
    for (int i = 0; i < n; i++) {
        dequeueDynamicQueue(&q, &aux);
        g_sink += aux.rpm;
    }

    for (int j = 0; j < REPS; j++) {
        initDynamicQueue(&q);
        clock_gettime(CLOCK_MONOTONIC, &a);
        for (int i = 0; i < n; i++) enqueueDynamicQueue(&q, data[i]);
        clock_gettime(CLOCK_MONOTONIC, &b);
        et[j] = elapsed_ns(a, b);

        clock_gettime(CLOCK_MONOTONIC, &a);
        for (int i = 0; i < n; i++) {
            dequeueDynamicQueue(&q, &aux);
            g_sink += aux.rpm;
        }
        clock_gettime(CLOCK_MONOTONIC, &b);
        dt[j] = elapsed_ns(a, b);

        initDynamicQueue(&q);
        clock_gettime(CLOCK_MONOTONIC, &a);
        for (int i = 0; i < n; i++) {
            enqueueDynamicQueue(&q, data[i]);
            dequeueDynamicQueue(&q, &aux);
            g_sink += aux.rpm;
        }
        clock_gettime(CLOCK_MONOTONIC, &b);
        ct[j] = elapsed_ns(a, b);
    }

    *enq = median(et, REPS);
    *deq = median(dt, REPS);
    *cyc = median(ct, REPS);
}

int main (void) {
    initEngineSimulation();

    // Pré-gera N_MAX amostras uma unica vez (fora da medição)
    EngineSensorsData* data = (EngineSensorsData*) malloc (N_MAX * sizeof(EngineSensorsData));
    if (data == NULL) {
        fprintf(stderr, "malloc falhou\n");
        return 1;
    }
    for (int i = 0; i < N_MAX; i++) data[i] = getNextSensorsDataBlock();

    FILE* csv = fopen("bench_queue.csv", "w");
    fprintf(csv, "structure,n,enqueue_ns_op,dequeue_ns_op,cycle_ns_op,mem_bytes\n");

    size_t elem = sizeof(EngineSensorsData);
    size_t node = sizeof(DynamicQueueNode);
    long long stat_mem = (long long) sizeof(StaticCircularQueue);

    printf("\nsizeof(EngineSensorsData)=%zu B | sizeof(DynamicQueueNode)=%zu B | sizeof(StaticCircularQueue)=%lld B\n\n", elem, node, stat_mem);
    printf("%-9s %6s %12s %12s %12s %12s\n", "structure", "n", "enq(ns/op)", "deq(ns/op)", "ciclo(ns/op)", "mem(bytes)");

    for (int n = N_MIN; n <= N_MAX; n += N_STEP) {
        long long se, sd, sc, de, dd, dc;
        benchStatic(data, n, &se, &sd, &sc);
        benchDynamic(data, n, &de, &dd, &dc);

        double s_e = (double) se / n, s_d = (double) sd / n, s_c = (double) sc / n;
        double d_e = (double) de / n, d_d = (double) dd / n, d_c = (double) dc / n;
        long long dyn_mem = (long long) n * (long long) node;

        fprintf(csv, "static,%d,%.3f,%.3f,%.3f,%lld\n", n, s_e, s_d, s_c, stat_mem);
        fprintf(csv, "dynamic,%d,%.3f,%.3f,%.3f,%lld\n", n, d_e, d_d, d_c, dyn_mem);

        printf("%-9s %6d %12.3f %12.3f %12.3f %12lld\n", "static", n, s_e, s_d, s_c, stat_mem);
        printf("%-9s %6d %12.3f %12.3f %12.3f %12lld\n", "dynamic", n, d_e, d_d, d_c, dyn_mem);
    }

    fclose(csv);
    free(data);
    return 0;
}