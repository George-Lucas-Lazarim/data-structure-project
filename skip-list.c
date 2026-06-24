#include <stdio.h>
#include <stdlib.h>
#include "skip-list.h"

static uint8_t randomLevel (void) {
    uint8_t level = 1;

    while (level < SKIPLIST_MAX_LEVEL && rand() < SKIPLIST_P_THRESHOLD) level++;
    
    return level;
}

static SkipListNode* createSkipListNode (EngineSensorsData data, uint8_t level) {
    SkipListNode* newNode = (SkipListNode*) malloc (sizeof(SkipListNode));

    if (newNode == NULL) return NULL;

    newNode->forward = (SkipListNode**) malloc (level * sizeof(SkipListNode));

    if (newNode->forward == NULL) {
        free(newNode);
        return NULL;
    }

    newNode->data = data;
    newNode->level = level;

    for (int i = 0; i < level; i++) newNode->forward[i] = NULL;

    return newNode;
}