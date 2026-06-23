#include <stdio.h>
#include <stdlib.h>
#include "hash.h"

void initDTCHashTable (DTCHashTable *h, uint16_t size) {
    if (h == NULL) {
        printf("\nErro! O ponteiro da hash esta apontando para NULL");
        return;
    }

    h->table = (HashNode**) malloc (size * sizeof(HashNode*));

    if (h->table == NULL) return;

    h->size = size;
    h->total_elements = 0;

    for (int i = 0; i < size; i++) h->table[i] = NULL;
}