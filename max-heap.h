#ifndef MAX_HEAP_H
#define MAX_HEAP_H

#include <stdint.h>
#include <stdbool.h>
#include "hash.h" // struct DTCAlert

typedef struct {
    DTCAlert** data;
    uint16_t capacity;
    uint16_t size;
} MaxHeap;

// Funções básicas
void initMaxHeap (MaxHeap* mh, uint16_t capacity);
bool insertMaxHeap (MaxHeap* mh, DTCAlert* alert);
DTCAlert* extractMax (MaxHeap* mh);
void freeMaxHeap (MaxHeap* mh);

#endif