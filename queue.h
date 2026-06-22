#ifndef QUEUE_H
#define QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include "synthetic-data.h"

// Fila estática
#define QUEUE_SIZE 100

typedef struct {
    EngineSensorsData data[QUEUE_SIZE];
    uint8_t start;
    uint8_t end;
    uint8_t total_elements;
} StaticCircularQueue;

// Funções básicas
void initStaticCircularQueue (StaticCircularQueue *q);
bool enqueueStaticCircularQueue (StaticCircularQueue *q, EngineSensorsData data);
bool dequeueStaticCircularQueue (StaticCircularQueue *q, EngineSensorsData *data);
bool searchDataStaticCircularQueue (StaticCircularQueue *q, uint32_t time, EngineSensorsData *data);

// Funções adicionais
float averageRPMStaticCircularQueue (StaticCircularQueue *q);
float temperatureRateStaticCircularQueue (StaticCircularQueue *q);
float boostSurgeStaticCircularQueue (StaticCircularQueue *q);

// Fila dinâmica
typedef struct {
    EngineSensorsData data;
    struct DynamicQueueNode *next;
} DynamicQueueNode;

typedef struct {
    DynamicQueueNode *start;
    DynamicQueueNode *end;
    uint32_t total_elements;
} DynamicQueue;

// Funções básicas
void initDynamicQueue (DynamicQueue *q);
bool enqueueDynamicQueue (DynamicQueue *q, EngineSensorsData data);
bool dequeueDynamicQueue (DynamicQueue *q, EngineSensorsData *data);
bool searchDataDynamicQueue (DynamicQueue *q, uint32_t time, EngineSensorsData *data);

// Funções adicionais
float averageRPMDynamicQueue (DynamicQueue *q);
float temperatureRateDynamicQueue (DynamicQueue *q);
float boostSurgeDynamicQueue (DynamicQueue *q);

#endif