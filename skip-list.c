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

void initSkipList (SkipList* sl) {
    if (sl == NULL) {
        printf("\nErro! O ponteiro da skip list esta apontando para NULL");
        return;
    }

    sl->header = (SkipListNode*) malloc (sizeof(SkipListNode));

    if (sl->header == NULL) return;

    sl->header->forward = (SkipListNode**) malloc (SKIPLIST_MAX_LEVEL * sizeof(SkipListNode*));

    if (sl->header->forward == NULL) {
        free(sl->header);
        sl->header = NULL;
        return;
    }

    for (int i = 0; i < SKIPLIST_MAX_LEVEL; i++) sl->header->forward[i] = NULL;

    sl->header->level = SKIPLIST_MAX_LEVEL;
    sl->level = 1;
    sl->total_elements = 0;
}

bool insertSkipList (SkipList* sl, EngineSensorsData data) {
    if (sl == NULL || sl->header == NULL) {
        printf("\nErro! A skip list nao foi inicializada");
        return false;
    }

    SkipListNode* update[SKIPLIST_MAX_LEVEL];
    SkipListNode* current = sl->header;

    for (int i = sl->level - 1; i >= 0; i--) {
        while (current->forward[i] != NULL && current->forward[i]->data.time_ms) {
            current = current->forward[i];

            update[i] = current;
        }
    }

    current = current->forward[0];

    if (current != NULL && current->data.time_ms == data.time_ms) {
        current->data = data;
        return true;
    }

    uint8_t new_level = randomLevel();

    if (new_level > sl->level) {
        for (int i = sl->level; i < new_level; i++) update[i] = sl->header;
        sl->level = new_level;
    }

    SkipListNode* newNode = createSkipListNode(data, new_level);

    if (newNode == NULL) return false;

    for (int i = 0; i < new_level; i++) {
        newNode->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = newNode;
    }

    sl->total_elements++;

    return true;
}

bool searchSkipList (SkipList* sl, uint32_t time_ms, EngineSensorsData* out) {
    if (sl == NULL || sl->header == NULL) {
        printf("\nErro! A skip list nao foi inicializada");
        return false;
    }
 
    SkipListNode* current = sl->header;
 
    for (int i = sl->level - 1; i >= 0; i--) {
        while (current->forward[i] != NULL && current->forward[i]->data.time_ms < time_ms)
            current = current->forward[i];
    }
 
    current = current->forward[0];
 
    if (current != NULL && current->data.time_ms == time_ms) {
        if (out != NULL) *out = current->data;
        return true;
    }
 
    return false;
}

bool removeSkipList (SkipList* sl, uint32_t time_ms) {
    if (sl == NULL || sl->header == NULL) {
        printf("\nErro! A skip list nao foi inicializada");
        return false;
    }
 
    SkipListNode* update[SKIPLIST_MAX_LEVEL];
    SkipListNode* current = sl->header;
 
    for (int i = sl->level - 1; i >= 0; i--) {
        while (current->forward[i] != NULL && current->forward[i]->data.time_ms < time_ms)
            current = current->forward[i];
 
        update[i] = current;
    }
 
    current = current->forward[0];
 
    if (current == NULL || current->data.time_ms != time_ms) return false;
 
    for (int i = 0; i < sl->level; i++) {
        if (update[i]->forward[i] != current) break;
        update[i]->forward[i] = current->forward[i];
    }
 
    free(current->forward);
    free(current);
 
    while (sl->level > 1 && sl->header->forward[sl->level - 1] == NULL) sl->level--;
 
    sl->total_elements--;
 
    return true;
}

void freeSkipList (SkipList* sl) {
    if (sl == NULL || sl->header == NULL) return;
 
    SkipListNode* current = sl->header->forward[0];
 
    while (current != NULL) {
        SkipListNode* next = current->forward[0];
 
        free(current->forward);
        free(current);
 
        current = next;
    }
 
    free(sl->header->forward);
    free(sl->header);
 
    sl->header = NULL;
    sl->level = 0;
    sl->total_elements = 0;
}
