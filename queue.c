#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

void initStaticCircularQueue (StaticCircularQueue *q) {
    if (q == NULL) {
        printf("\nErro! O ponteiro da fila esta apontando para NULL");
        return;
    }

    q->start = 0;
    q->end = -1;
    q->total_elements = 0;
}

bool enqueueStaticCircularQueue (StaticCircularQueue *q, EngineSensorsData data) {
    if (q == NULL) {
        printf("\nErro! A fila nao foi inicializada");
        return false;
    } else if (q->total_elements == QUEUE_SIZE) return false;

    q->end = (q->end + 1) % QUEUE_SIZE; // Nunca passa de (QUEUE_SIZE - 1)

    q->data[q->end] = data;
    q->total_elements++;

    return true;
}

bool dequeueStaticCircularQueue (StaticCircularQueue *q, EngineSensorsData *data) {
    if (q == NULL) {
        printf("\nErro! A fila nao foi inicializada");
        return false;
    } else if (q->total_elements == 0) return false;

    *data = q->data[q->start];

    q->start = (q->start + 1) % QUEUE_SIZE;
    q->total_elements--;

    return true;
}

bool searchDataStaticCircularQueue (StaticCircularQueue *q, uint32_t time, EngineSensorsData *data) {
    if (q == NULL) {
        printf("\nErro! A fila nao foi inicializada");
        return false;
    } else if (q->total_elements == 0) return false;

    uint16_t i = q->start;

    for (int j = 0; j < q->total_elements; j++) {
        if (q->data[i].time_ms == time) {
            *data = q->data[i];
            return true;
        } else i = (i + 1) % QUEUE_SIZE;
    }

    return false;
}