#include <stdio.h>
#include "max-heap.h"

void initMaxHeap (MaxHeap* mh, uint16_t capacity) {
    if (mh == NULL) {
        printf("\nErro! O ponteiro da max-heap esta apontando para NULL");
        return;
    } else if (capacity == 0) return;

    mh->data = (DTCAlert**) malloc (capacity * sizeof(DTCAlert*));

    if (mh->data == NULL) return;

    mh->capacity = capacity;
    mh->size = 0;

    for (int i = 0; i < capacity; i++) mh->data[i] = NULL;
}