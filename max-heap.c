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

bool insertMaxHeap (MaxHeap* mh, DTCAlert* alert) {
    if (mh == NULL) {
        printf("\nErro! A max-heap nao foi inicializada");
        return false;
    } else if (mh->data == NULL || mh->size == mh->capacity) return false;

    mh->data[mh->size] = alert;

    uint16_t i = mh->size;
    uint16_t parent_i = (i - 1) / 2;

    mh->size++;

    while (i > 0 && mh->data[i]->severity > mh->data[parent_i]->severity) {
        DTCAlert* aux = mh->data[parent_i];

        mh->data[parent_i] = mh->data[i];
        mh->data[i] = aux;

        i = parent_i;
        parent_i = (i - 1) / 2;
    }

    return true;
}

DTCAlert extractMax (MaxHeap* mh) {
    if (mh == NULL || mh->data == NULL || mh->size == 0) {
        printf("\nErro! A max-heap nao foi inicializada");
        DTCAlert empty = {0}; // struct vazia (preenchida com zeros)
        return empty;
    }

    DTCAlert extracted = *(mh->data[0]);

    free(mh->data[0]);
    mh->size--;

    if (mh->size == 0) return extracted;

    mh->data[0] = mh->data[mh->size];
    mh->data[mh->size] = NULL;

    uint16_t i = 0;

    while (true) {
        uint16_t parent = i;
        uint16_t son_left = (2 * i) + 1;
        uint16_t son_right = (2 * i) + 2;

        if (son_left < mh->size && mh->data[son_left]->severity > mh->data[parent]->severity) parent = son_left;

        if (son_right < mh->size && mh->data[son_right]->severity > mh->data[parent]->severity) parent = son_right;

        if (parent == i) break;

        DTCAlert* aux = mh->data[i];
        mh->data[i] = mh->data[parent];
        mh->data[parent] = aux;

        i = parent;
    }
    
    return extracted;
}