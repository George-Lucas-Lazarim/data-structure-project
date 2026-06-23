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

int calculateHash (DTCHashTable *h, uint16_t raw_code) {
    return (h == NULL || h->size == 0) ? -1 : raw_code % h->size;
}

bool insertDTC (DTCHashTable *h, DTCAlert alert) {
    if (h == NULL) {
        printf("\nErro! A hash nao foi inicializada");
        return false;
    }

    int i = calculateHash(h, alert.raw_code);

    if (i == -1) return false;

    HashNode* newNode = (HashNode*) malloc (sizeof(HashNode));

    if (newNode == NULL) return false;

    newNode->alert = alert;
    newNode->next = h->table[i];

    h->table[i] = newNode;
    h->total_elements++;

    return true;
}

bool removeDTC (DTCHashTable *h, uint16_t raw_code) {
    if (h == NULL) {
        printf("\nErro! A hash nao foi inicializada");
        return false;
    } else if (h->total_elements == 0) return false;

    int i = calculateHash(h, raw_code);

    HashNode* aux = NULL;
    HashNode* current = h->table[i];

    while (current != NULL) {
        if (current->alert.raw_code == raw_code) {
            if (aux == NULL) h->table[i] = current->next;
            else aux->next = current->next;

            free(current);
            h->total_elements--;

            return true;
        }
        
        aux = current;
        current = current->next;
    }

    return false;
}