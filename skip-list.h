#ifndef SKIP_LIST_H
#define SKIP_LIST_H

#include <stdint.h>
#include <stdbool.h>
#include "synthetic-data.h" // struct EngineSensorsData

#define SKIPLIST_MAX_LEVEL 17

#ifndef SKIPLIST_P_THRESHOLD
#define SKIPLIST_P_THRESHOLD (RAND_MAX / 2) // p ~ 0.5 Por padrão
#endif

typedef struct SkipListNode {
    EngineSensorsData data;
    struct SkipListNode** forward;
    uint8_t level;
} SkipListNode;

typedef struct {
    SkipListNode* header;
    uint8_t level;
    uint32_t total_elements;
} SkipList;

// Funções básicas
void initSkipList (SkipList* sl);
bool insertSkipList (SkipList* sl, EngineSensorsData data);
bool searchSkipList (SkipList* sl, uint32_t time_ms, EngineSensorsData* out);
bool removeSkipList (SkipList* sl, uint32_t time_ms);
void freeSkipList (SkipList* sl);

#endif