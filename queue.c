#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

void initStaticCircularQueue (StaticCircularQueue *q) {
    if (q == NULL) {
        printf("\nErro! O ponteiro da fila circular estatica esta apontando para NULL");
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

float averageRPMStaticCircularQueue (StaticCircularQueue *q) {
    if (q == NULL) {
        printf("\nErro! A fila nao foi inicializada");
        return 0.0f;
    } else if (q->total_elements == 0) return 0.0f;

    uint32_t rpm_sum = 0;

    for (int j = 0; j < q->total_elements; j++) rpm_sum += q->data[(q->start + j) % QUEUE_SIZE].rpm;

    return (float) rpm_sum / q->total_elements;
}

float temperatureRateStaticCircularQueue (StaticCircularQueue *q) {
    if (q == NULL) {
        printf("\nErro! A fila nao foi inicializada");
        return 0.0f;
    } else if (q->total_elements < 2) return 0.0f;

    float delta_temp = q->data[q->end].temperature - q->data[q->start].temperature;
    uint32_t delta_time_ms = q->data[q->end].time_ms - q->data[q->start].time_ms;

    if (delta_time_ms == 0) return 0.0f;

    return (float) 1000 * delta_temp / delta_time_ms; // °C/s
}

float maxTurboPressureStaticCircularQueue (StaticCircularQueue *q) {
    if (q == NULL) {
        printf("\nErro! A fila nao foi inicializada");
        return 0.0f;
    } else if (q->total_elements == 0) return 0.0f;

    float max_turbo_pressure = -100.0f;

    for (int i = 0; i < q->total_elements; i++) {
        float current_turbo_pressure = q->data[(q->start + i) % QUEUE_SIZE].turbo_pressure;

        if (current_turbo_pressure > max_turbo_pressure) max_turbo_pressure = current_turbo_pressure;
    }

    return max_turbo_pressure;
}

void initDynamicQueue (DynamicQueue *q) {
    if (q == NULL) {
        printf("\nErro! O ponteiro da fila dinamica esta apontando para NULL");
        return;
    }

    q->start = NULL;
    q->end = NULL;
    q->total_elements = 0;
}

bool enqueueDynamicQueue (DynamicQueue *q, EngineSensorsData data) {
    if (q == NULL) {
        printf("\nErro! A fila nao foi inicializada");
        return false;
    }

    DynamicQueueNode *new_node = (DynamicQueueNode*) malloc (sizeof(DynamicQueueNode));
    if (new_node == NULL) return false;

    new_node->data = data;
    new_node->next = NULL;

    if (q->total_elements == 0) q->start = new_node;
    else q->end->next = new_node;

    q->end = new_node;
    q->total_elements++;

    return true;
}

bool dequeueDynamicQueue (DynamicQueue *q, EngineSensorsData *data) {
    if (q == NULL) {
        printf("\nErro! A fila nao foi inicializada");
        return false;
    } else if (q->total_elements == 0) return false;

    DynamicQueueNode *aux = q->start;
    *data = aux->data;

    q->start = q->start->next;
    
    if (q->start == NULL) q->end = NULL;

    free(aux);
    q->total_elements--;

    return true;
}

bool searchDataDynamicQueue (DynamicQueue *q, uint32_t time, EngineSensorsData *data) {
    if (q == NULL) {
        printf("\nErro! A fila nao foi inicializada");
        return false;
    } else if (q->total_elements == 0 || time < 0) return false;

    DynamicQueueNode *aux = q->start;

    while (aux != NULL) {
        if (aux->data.time_ms == time) {
            *data = aux->data;
            return true;
        } else aux = aux->next;
    }

    return false;
}

float averageRPMDynamicQueue (DynamicQueue *q) {
    if (q == NULL) {
        printf("\nErro! A fila nao foi inicializada");
        return 0.0f;
    } else if (q->total_elements == 0) return 0.0f;

    uint32_t rpm_sum = 0;
    DynamicQueueNode *aux = q->start;

    while (aux != NULL) {
        rpm_sum += aux->data.rpm;
        aux = aux->next;
    }

    return (float) rpm_sum / q->total_elements;
}

float temperatureRateDynamicQueue (DynamicQueue *q) {
    if (q == NULL) {
        printf("\nErro! A fila nao foi inicializada");
        return 0.0f;
    } else if (q->total_elements < 2) return 0.0f;

    float delta_temp = q->end->data.temperature - q->start->data.temperature;
    uint32_t delta_time_ms = q->end->data.time_ms - q->start->data.time_ms;

    if (delta_time_ms == 0) return 0.0f;

    return (float) 1000 * delta_temp / delta_time_ms; // °C/s
}

float maxTurboPressureDynamicQueue (DynamicQueue *q) {
    if (q == NULL) {
        printf("\nErro! A fila nao foi inicializada");
        return 0.0f;
    } else if (q->total_elements == 0) return 0.0f;

    float max_turbo_pressure = -100.0f;
    DynamicQueueNode *aux = q->start;

    while (aux != NULL) {
        if (aux->data.turbo_pressure > max_turbo_pressure) max_turbo_pressure = aux->data.turbo_pressure;
        aux = aux->next;
    }

    return max_turbo_pressure;
}