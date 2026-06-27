// Compilar:  gcc -Wall -Wextra -std=c11 -O2 *.c -lm -o main
// Rodar normal:                ./main
// Com restricao R5 (memoria):  gcc -Wall -Wextra -std=c11 -O2 -DHISTORY_LIMIT=5000 *.c -lm -o main && ./main
// Com R1 (128 MB) + R6 (1 nucleo):  ( ulimit -v 131072 ; taskset -c 0 ./main )
//   -> navegue a vontade e digite 'q'; o pico de memoria aparece ao encerrar.

// https://claude.ai/share/8ebf8648-63e2-495e-9768-d12845d95fcc Conversa com o Claude
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/resource.h> // getrusage -> pico de memoria (RSS)

#include "synthetic-data.h"
#include "queue.h"
#include "hash.h"
#include "bloom-filter.h"
#include "max-heap.h"
#include "skip-list.h"

// ===================== Cores ANSI =====================
#define A_RESET "\x1b[0m"
#define A_BOLD  "\x1b[1m"
#define A_DIM   "\x1b[2m"
#define A_RED   "\x1b[31m"
#define A_GRN   "\x1b[32m"
#define A_YEL   "\x1b[33m"
#define A_BLU   "\x1b[34m"
#define A_MAG   "\x1b[35m"
#define A_CYN   "\x1b[36m"

// ===================== Parametros =====================
#define TOTAL_SAMPLES   20000u
#define CONSUME_PER_LOOP 32u
#define PRODUCE_MAX      64u

// R5: limite do historico. 0 = completo; >0 = descarta os mais antigos (janela deslizante).
#ifndef HISTORY_LIMIT
#define HISTORY_LIMIT 0
#endif

#define HASH_SIZE       64u
#define BLOOM_BITS      512u
#define BLOOM_HASHES    3u
#define HEAP_CAPACITY   256u

#define OIL_LOW_BAR     1.0f
#define OIL_RPM_MIN     2000u
#define TEMP_WARN_C     100.0f
#define TEMP_CRIT_C     105.0f
#define BOOST_MAX_BAR   1.2f
#define RPM_DANGER      6800u

#define WINDOW_MAX 20u
#define N_OIL   5u
#define M_OIL   3u
#define N_DEAD  3u
#define M_DEAD  2u
#define N_HEAT  15u
#define M_HEAT  10u
#define N_BOOST 5u
#define M_BOOST 3u
#define N_RPM   8u
#define M_RPM   5u
#define N_TURBO 12u
#define K_TURBO 3u

#define N_PRED     15u
#define HORIZON_MS 1000u
#define SLOPE_EPS  1e-5f

#define RAW_OIL_LOW       524u
#define RAW_ENGINE_DEAD   1300u
#define RAW_OVERHEAT      217u
#define RAW_HEAT_WARN     116u
#define RAW_OVERBOOST     234u
#define RAW_RPM_DANGER    219u
#define RAW_PRED_OVERHEAT 900u
#define RAW_PRED_OILLOSS  901u
#define RAW_TURBO_FAIL    902u

// Falha de turbina: o motor pede pressao (TPS e RPM altos) mas o boost nao acompanha,
// de forma SUSTENTADA. A chave e exigir uma CORRIDA consecutiva: com a turbina boa, o
// boost vira positivo ja na 2a amostra consecutiva de aceleracao (no gerador, +0.37/amostra),
// entao nunca ha 3 seguidas com boost <= 0 sob aceleracao. Com a turbina morta, ha.
// Isso evita o falso positivo do turbo "ainda nao spoolado" (que dura 1-2 amostras).
#define TURBO_TPS_MIN    20u     // pisou no acelerador
#define TURBO_RPM_MIN    3000u   // RPM alto: turbina deveria estar soprando
#define TURBO_BOOST_FAIL 0.0f    // sob aceleracao + RPM alto, boost <= 0 e anormal

// ===================== Rastreador de primeira deteccao =====================
#define MAX_TRACK 16
static uint16_t track_code[MAX_TRACK];
static uint32_t track_time[MAX_TRACK];
static int track_n = 0;

static void recordFirstSeen (uint16_t code, uint32_t t) {
    for (int i = 0; i < track_n; i++) if (track_code[i] == code) return;
    if (track_n < MAX_TRACK) { track_code[track_n] = code; track_time[track_n] = t; track_n++; }
}
static bool firstSeenTime (uint16_t code, uint32_t* out) {
    for (int i = 0; i < track_n; i++) if (track_code[i] == code) { *out = track_time[i]; return true; }
    return false;
}

// ===================== Janela de deteccao =====================
typedef struct {
    EngineSensorsData samples[WINDOW_MAX];
    uint8_t count;
    uint8_t head;
} DetectionWindow;

static void initWindow (DetectionWindow* w) { w->count = 0; w->head = WINDOW_MAX - 1; }
static void pushWindow (DetectionWindow* w, EngineSensorsData s) {
    w->head = (uint8_t)((w->head + 1u) % WINDOW_MAX);
    w->samples[w->head] = s;
    if (w->count < WINDOW_MAX) w->count++;
}
static uint8_t countRecent (const DetectionWindow* w, uint8_t n, bool (*pred)(const EngineSensorsData*)) {
    if (n > w->count) n = w->count;
    uint8_t c = 0;
    for (uint8_t i = 0; i < n; i++) {
        uint8_t idx = (uint8_t)((w->head - i + WINDOW_MAX) % WINDOW_MAX);
        if (pred(&w->samples[idx])) c++;
    }
    return c;
}
// Maior sequencia de amostras CONSECUTIVAS (nas ultimas n) que satisfazem o predicado.
static uint8_t longestRun (const DetectionWindow* w, uint8_t n, bool (*pred)(const EngineSensorsData*)) {
    if (n > w->count) n = w->count;
    uint8_t best = 0, cur = 0;
    for (uint8_t i = 0; i < n; i++) {
        uint8_t idx = (uint8_t)((w->head - i + WINDOW_MAX) % WINDOW_MAX);
        if (pred(&w->samples[idx])) { cur++; if (cur > best) best = cur; }
        else cur = 0;
    }
    return best;
}
static float windowSlope (const DetectionWindow* w, uint8_t n, float (*gety)(const EngineSensorsData*)) {
    if (n > w->count) n = w->count;
    if (n < 2) return 0.0f;
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (uint8_t i = 0; i < n; i++) {
        uint8_t idx = (uint8_t)((w->head - i + WINDOW_MAX) % WINDOW_MAX);
        double x = (double) w->samples[idx].time_ms;
        double y = (double) gety(&w->samples[idx]);
        sx += x; sy += y; sxx += x * x; sxy += x * y;
    }
    double denom = (double) n * sxx - sx * sx;
    if (denom == 0.0) return 0.0f;
    return (float)(((double) n * sxy - sx * sy) / denom);
}

static bool  predEngineDead (const EngineSensorsData* s) { return s->oil_pressure <= 0.0f && s->rpm > 0u; }
static bool  predOilLow     (const EngineSensorsData* s) { return s->oil_pressure < OIL_LOW_BAR && s->rpm > OIL_RPM_MIN; }
static bool  predOverheat   (const EngineSensorsData* s) { return s->temperature > TEMP_CRIT_C; }
static bool  predHeatWarn   (const EngineSensorsData* s) { return s->temperature > TEMP_WARN_C; }
static bool  predOverboost  (const EngineSensorsData* s) { return s->turbo_pressure > BOOST_MAX_BAR; }
static bool  predRpmDanger  (const EngineSensorsData* s) { return s->rpm > RPM_DANGER; }
// Turbina sob suspeita NESTA amostra: acelerando, RPM alto, mas sem pressao positiva.
static bool  predTurboFail  (const EngineSensorsData* s) {
    return s->tps > TURBO_TPS_MIN && s->rpm > TURBO_RPM_MIN && s->turbo_pressure <= TURBO_BOOST_FAIL;
}
static float getTemp        (const EngineSensorsData* s) { return s->temperature; }
static float getOil         (const EngineSensorsData* s) { return s->oil_pressure; }

// ===================== Hash =====================
static void populateDTCDictionary (DTCHashTable* h) {
    DTCAlert real[] = {
        { RAW_OIL_LOW,       "P0524", "Pressao de oleo muito baixa",          5 },
        { RAW_ENGINE_DEAD,   "P1300", "Falha critica - motor sem oleo",       5 },
        { RAW_OVERHEAT,      "P0217", "Superaquecimento do motor",            4 },
        { RAW_HEAT_WARN,     "P0116", "Atencao: temperatura elevada",         3 },
        { RAW_OVERBOOST,     "P0234", "Sobrepressao do turbo (overboost)",    3 },
        { RAW_RPM_DANGER,    "P0219", "Rotacao excessiva (overrev)",          2 },
        { RAW_PRED_OVERHEAT, "PR217", "Superaquecimento iminente (previsto)", 3 },
        { RAW_PRED_OILLOSS,  "PR524", "Perda de pressao iminente (prevista)", 4 },
        { RAW_TURBO_FAIL,    "P0299", "Falha de turbina (sem pressao)",       4 },
    };
    int nr = (int)(sizeof(real) / sizeof(real[0]));
    for (int i = 0; i < nr; i++) insertDTC(h, real[i]);

    uint16_t buckets[] = { 0, 1, 3, 6 };
    int nb = (int)(sizeof(buckets) / sizeof(buckets[0]));
    for (int b = 0; b < nb; b++) {
        for (uint16_t k = 1; k <= 6; k++) {
            DTCAlert filler;
            filler.raw_code = (uint16_t)(buckets[b] + k * HASH_SIZE);
            strcpy(filler.obd2_code, "FILL");
            strcpy(filler.description, "reservado (colisao induzida)");
            filler.severity = 1;
            insertDTC(h, filler);
        }
    }
}

// ===================== Pipeline =====================
static void raiseAlert (uint16_t code, uint32_t now, DTCHashTable* h, BloomFilter* bf, MaxHeap* heap) {
    if (checkBloomFilter(bf, code)) return;
    insertBloomFilter(bf, code);
    recordFirstSeen(code, now);
    DTCAlert* d = searchDTC(h, code);
    if (d != NULL) insertMaxHeap(heap, d);
}
static void detectAnomalies (const DetectionWindow* w, uint32_t now, DTCHashTable* h, BloomFilter* bf, MaxHeap* heap) {
    if (countRecent(w, N_DEAD,  predEngineDead) >= M_DEAD)  raiseAlert(RAW_ENGINE_DEAD, now, h, bf, heap);
    if (countRecent(w, N_OIL,   predOilLow)     >= M_OIL)   raiseAlert(RAW_OIL_LOW,     now, h, bf, heap);
    if (countRecent(w, N_HEAT,  predOverheat)   >= M_HEAT)  raiseAlert(RAW_OVERHEAT,    now, h, bf, heap);
    if (countRecent(w, N_HEAT,  predHeatWarn)   >= M_HEAT)  raiseAlert(RAW_HEAT_WARN,   now, h, bf, heap);
    if (countRecent(w, N_BOOST, predOverboost)  >= M_BOOST) raiseAlert(RAW_OVERBOOST,   now, h, bf, heap);
    if (countRecent(w, N_RPM,   predRpmDanger)  >= M_RPM)   raiseAlert(RAW_RPM_DANGER,  now, h, bf, heap);
    if (longestRun(w, N_TURBO, predTurboFail)   >= K_TURBO) raiseAlert(RAW_TURBO_FAIL,  now, h, bf, heap);
}
static void predictFailures (const DetectionWindow* w, uint32_t now, DTCHashTable* h, BloomFilter* bf, MaxHeap* heap) {
    if (w->count < N_PRED) return;
    const EngineSensorsData* cur = &w->samples[w->head];
    float st = windowSlope(w, N_PRED, getTemp);
    if (cur->temperature < TEMP_CRIT_C && st > SLOPE_EPS) {
        float ttl = (TEMP_CRIT_C - cur->temperature) / st;
        if (ttl > 0.0f && ttl < (float) HORIZON_MS) raiseAlert(RAW_PRED_OVERHEAT, now, h, bf, heap);
    }
    float sp = windowSlope(w, N_PRED, getOil);
    if (cur->oil_pressure > OIL_LOW_BAR && cur->rpm > OIL_RPM_MIN && sp < -SLOPE_EPS) {
        float ttl = (OIL_LOW_BAR - cur->oil_pressure) / sp;
        if (ttl > 0.0f && ttl < (float) HORIZON_MS) raiseAlert(RAW_PRED_OILLOSS, now, h, bf, heap);
    }
}

static const char* sevColor (uint8_t s, int predicted) {
    if (predicted) return A_MAG;
    switch (s) {
        case 5:  return A_BOLD A_RED;
        case 4:  return A_RED;
        case 3:  return A_YEL;
        case 2:  return A_CYN;
        default: return A_RESET;
    }
}

// ===================== Visualizacao (gnuplot via popen) =====================
// Dashboard 2x2 a partir do telemetry.csv. break_ms > 0 marca o instante da quebra.
static void showDashboard (uint32_t break_ms) {
    if (system("which gnuplot > /dev/null 2>&1") != 0) {
        printf(A_YEL "\n(gnuplot nao encontrado; pulei o grafico. Instale: sudo apt install gnuplot)\n" A_RESET);
        return;
    }

    // Render para PNG: o terminal interativo 'qt' crasha nesta instalacao
    // (gnuplot_qt: trap int3 no glib). Usamos o terminal 'png' (libgd), que
    // nao abre janela GUI e e leve: cabe em ~128 MB de memoria virtual.
    // (pngcairo puxa cairo/fontconfig e precisa de ~256 MB, segfaultando sob
    // ulimit -v 131072.) GNUTERM=png evita inicializar o qt no startup.
    FILE* gp = popen("GNUTERM=png gnuplot", "w");
    if (gp == NULL) return;

    fprintf(gp, "set terminal png size 1280,960\n");
    fprintf(gp, "set output 'dashboard.png'\n");

    char vbar[160]; vbar[0] = '\0';
    if (break_ms > 0)
        snprintf(vbar, sizeof vbar,
                 "set arrow 9 from %u, graph 0 to %u, graph 1 nohead lc rgb '#000000' dt 3\n",
                 break_ms, break_ms);

    fprintf(gp, "set datafile separator ','\n");
    fprintf(gp, "unset key\n");
    fprintf(gp, "set multiplot layout 2,2 title 'Telemetria ECU - AP 1.6 8v turbo (linha pontilhada = quebra)'\n");
    fprintf(gp, "set xlabel 'tempo (ms)'\n");

    fprintf(gp, "set ylabel 'RPM'\n%s", vbar);
    fprintf(gp, "plot 'telemetry.csv' every ::1 using 1:2 with lines lc rgb '#185FA5'\n");

    fprintf(gp, "set ylabel 'temperatura (C)'\n");
    fprintf(gp, "set arrow 1 from graph 0, first 105 to graph 1, first 105 nohead lc rgb '#A32D2D' dt 2\n%s", vbar);
    fprintf(gp, "plot 'telemetry.csv' every ::1 using 1:5 with lines lc rgb '#BA7517'\n");
    fprintf(gp, "unset arrow 1\n");

    fprintf(gp, "set ylabel 'pressao oleo (bar)'\n");
    fprintf(gp, "set arrow 2 from graph 0, first 1.0 to graph 1, first 1.0 nohead lc rgb '#A32D2D' dt 2\n%s", vbar);
    fprintf(gp, "plot 'telemetry.csv' every ::1 using 1:4 with lines lc rgb '#0F6E56'\n");
    fprintf(gp, "unset arrow 2\n");

    fprintf(gp, "set ylabel 'boost (bar)'\n%s", vbar);
    fprintf(gp, "plot 'telemetry.csv' every ::1 using 1:6 with lines lc rgb '#534AB7'\n");

    fprintf(gp, "unset multiplot\n");
    fprintf(gp, "set output\n");   // finaliza/fecha o arquivo PNG
    pclose(gp);

    printf(A_YEL "\n(grafico salvo em dashboard.png)\n" A_RESET);
    if (system("xdg-open dashboard.png > /dev/null 2>&1 &")) {}   // tenta abrir; ignora se falhar
}

// ===================== main =====================
int main (void) {
    initEngineSimulation();

    StaticCircularQueue queue;
    DTCHashTable hash;
    BloomFilter bloom;
    MaxHeap heap;
    SkipList history;
    DetectionWindow window;

    initStaticCircularQueue(&queue);
    initDTCHashTable(&hash, HASH_SIZE);
    populateDTCDictionary(&hash);
    initBloomFilter(&bloom, BLOOM_BITS, BLOOM_HASHES);
    initMaxHeap(&heap, HEAP_CAPACITY);
    initSkipList(&history);
    initWindow(&window);

    FILE* tele = fopen("telemetry.csv", "w");
    if (tele) fprintf(tele, "time_ms,rpm,tps,oil_pressure,temperature,turbo_pressure\n");

    printf(A_BOLD A_CYN "\n==============================================\n");
    printf("   ECU - Telemetria AP 1.6 8v Turbo\n");
    printf("==============================================\n" A_RESET);
#if HISTORY_LIMIT > 0
    printf(A_YEL "Restricao R5 ativa: historico limitado a %d pacotes (descarte dos antigos).\n" A_RESET, HISTORY_LIMIT);
#endif

    uint32_t produced = 0, loop = 0, last_time_ms = 0;

    while (produced < TOTAL_SAMPLES || queue.total_elements > 0) {
        if (produced < TOTAL_SAMPLES) {
            uint32_t burst = (uint32_t) rand() % (PRODUCE_MAX + 1u);
            for (uint32_t i = 0; i < burst && produced < TOTAL_SAMPLES
                                 && queue.total_elements < QUEUE_SIZE; i++) {
                EngineSensorsData s = getNextSensorsDataBlock();
                enqueueStaticCircularQueue(&queue, s);
                produced++;
                last_time_ms = s.time_ms;
            }
        }

        for (uint32_t c = 0; c < CONSUME_PER_LOOP; c++) {
            EngineSensorsData s;
            if (!dequeueStaticCircularQueue(&queue, &s)) break;

            insertSkipList(&history, s);
            if (tele) fprintf(tele, "%u,%u,%u,%.3f,%.2f,%.3f\n",
                              s.time_ms, s.rpm, s.tps, s.oil_pressure, s.temperature, s.turbo_pressure);

            // R5: se o historico passou do teto, descarta o mais antigo (front da skip list).
#if HISTORY_LIMIT > 0
            if (history.total_elements > HISTORY_LIMIT) {
                SkipListNode* oldest = history.header->forward[0];
                if (oldest != NULL) removeSkipList(&history, oldest->data.time_ms);
            }
#endif

            pushWindow(&window, s);
            detectAnomalies(&window, s.time_ms, &hash, &bloom, &heap);
            predictFailures(&window, s.time_ms, &hash, &bloom, &heap);
        }

        if (loop % 16 == 0) {
            printf("\r" A_DIM "processando... %u / %u amostras  (fila: %u | historico: %u)   " A_RESET,
                   produced, (unsigned) TOTAL_SAMPLES, queue.total_elements, history.total_elements);
            fflush(stdout);
        }
        loop++;
    }
    if (tele) fclose(tele);
    printf("\r" A_DIM "processamento concluido (%u amostras).                            \n" A_RESET, produced);

    // ===================== Resumo =====================
    printf(A_BOLD A_CYN "\n=== RESUMO DA EXECUCAO ===\n" A_RESET);
    printf("  Historico (skip list): %s%u%s pacotes\n", A_BOLD, history.total_elements, A_RESET);
    printf("  Hash: %u entradas | maior cadeia: %s%u%s | colisoes: %s%u%s\n",
           hash.total_elements, A_BOLD, getLongestChain(&hash), A_RESET,
           A_BOLD, getTotalCollisions(&hash), A_RESET);
    printf("  Bloom: ocupacao %.3f | FP estimado %.5f\n",
           getBloomFilterOccupancy(&bloom), estimateFalsePositiveRate(&bloom));

    // ===================== Painel de alertas =====================
    printf(A_BOLD A_CYN "\n=== PAINEL DE ALERTAS (heap, por severidade) ===\n" A_RESET);
    if (heap.size == 0) printf(A_DIM "  (nenhuma anomalia confirmada nesta execucao)\n" A_RESET);
    while (heap.size > 0) {
        DTCAlert* a = extractMax(&heap);
        if (a == NULL) break;
        int pred = (a->obd2_code[1] == 'R');
        const char* col = sevColor(a->severity, pred);
        printf("%s  %s [sev %u]  %-6s %s%s\n",
               col, pred ? "PREVISAO" : "ALERTA  ", a->severity, a->obd2_code, a->description, A_RESET);
        uint32_t t;
        EngineSensorsData snap;
        if (firstSeenTime(a->raw_code, &t) && searchSkipList(&history, t, &snap)) {
            printf(A_BOLD "      detectado em t=%u ms" A_RESET A_DIM
                   "  ->  RPM=%u | oleo=%.2f bar | temp=%.1f C | boost=%.2f bar\n" A_RESET,
                   t, snap.rpm, snap.oil_pressure, snap.temperature, snap.turbo_pressure);
            printf(A_DIM "      para ver o antes/depois, consulte t=%u ate t=%u na skip list\n" A_RESET,
                   (t >= 120u) ? (t - 120u) : 0u, t + 120u);
        }
    }

    // ===================== Grafico =====================
    // Marcador de quebra no grafico: prioriza a turbina (causa raiz), depois oleo.
    uint32_t break_ms = 0, tmp;
    if (firstSeenTime(RAW_TURBO_FAIL, &tmp)) break_ms = tmp;
    else if (firstSeenTime(RAW_ENGINE_DEAD, &tmp)) break_ms = tmp;
    else if (firstSeenTime(RAW_OIL_LOW, &tmp)) break_ms = tmp;
    showDashboard(break_ms);

    // ===================== Consulta interativa =====================
    printf(A_BOLD A_CYN "\n=== CONSULTA DE HISTORICO (skip list) ===\n" A_RESET);
    printf(A_DIM "Digite um tempo em ms (multiplos de 12, de 12 a %u) ou 'q' para sair.\n" A_RESET, last_time_ms);

    char line[64];
    printf("tempo> "); fflush(stdout);
    while (fgets(line, sizeof line, stdin) != NULL) {
        if (line[0] == 'q' || line[0] == 'Q' || strncmp(line, "sair", 4) == 0) break;
        char* e;
        unsigned long t = strtoul(line, &e, 10);
        if (e == line) {
            printf(A_YEL "  entrada invalida\n" A_RESET);
        } else {
            EngineSensorsData out;
            if (searchSkipList(&history, (uint32_t) t, &out))
                printf(A_GRN "  t=%lu ms -> RPM=%u | TPS=%u%% | oleo=%.2f bar | temp=%.1f C | boost=%.2f bar\n" A_RESET,
                       t, out.rpm, out.tps, out.oil_pressure, out.temperature, out.turbo_pressure);
            else
                printf(A_YEL "  nada em t=%lu (multiplos de 12; sob R5 os antigos sao descartados)\n" A_RESET, t);
        }
        printf("tempo> "); fflush(stdout);
    }
    printf(A_DIM "\nencerrando.\n" A_RESET);

    freeDTCHashTable(&hash);
    freeBloomFilter(&bloom);
    freeMaxHeap(&heap);
    freeSkipList(&history);

    // Pico de memoria residente do processo (RSS) - numero usado para documentar a R1.
    struct rusage ru;
    if (getrusage(RUSAGE_SELF, &ru) == 0) {
        long kb = ru.ru_maxrss; // no Linux, ja vem em kilobytes
        printf(A_BOLD A_CYN "\n=== MEMORIA ===\n" A_RESET);
        printf("  Pico de memoria residente (RSS): %s%ld KB%s (~%.2f MB)\n",
               A_BOLD, kb, A_RESET, kb / 1024.0);
        printf(A_DIM "  (teto da R1: 131072 KB = 128 MB)\n" A_RESET);
    }
    return 0;
}