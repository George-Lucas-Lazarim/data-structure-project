#include <stdio.h>
#include <stdlib.h>
#include "skip-list.h"

static uint8_t randomLevel (void) {
    uint8_t level = 1;

    while (level < SKIPLIST_MAX_LEVEL && rand() < SKIPLIST_P_THRESHOLD) level++;
    
    return level;
}