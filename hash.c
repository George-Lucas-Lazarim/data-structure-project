#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"

void initDTCHashTable (DTCHashTable* h, uint16_t size) {
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

int calculateHash (DTCHashTable* h, uint16_t raw_code) {
    return (h == NULL || h->size == 0) ? -1 : raw_code % h->size;
}

bool insertDTC (DTCHashTable* h, DTCAlert alert) {
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

bool removeDTC (DTCHashTable* h, uint16_t raw_code) {
    if (h == NULL) {
        printf("\nErro! A hash nao foi inicializada");
        return false;
    } else if (h->total_elements == 0) return false;

    int i = calculateHash(h, raw_code);

    if (i == -1) return false;

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

DTCAlert* searchDTC (DTCHashTable* h, uint16_t raw_code) {
    if (h == NULL) {
        printf("\nErro! A hash nao foi inicializada");
        return NULL;
    } else if (h->total_elements == 0) return NULL;

    int i = calculateHash(h, raw_code);

    if (i == -1) return NULL;

    HashNode* aux = h->table[i];

    while (aux != NULL) {
        if (aux->alert.raw_code == raw_code) return &(aux->alert);
        else aux = aux->next;
    }
    
    return NULL;
}

void freeDTCHashTable (DTCHashTable* h) {
    if (h == NULL || h->table == NULL) return;

    for (int i = 0; i < h->size; i++) {
        HashNode* aux;
        HashNode* current = h->table[i];

        while (current != NULL) {
            aux = current->next;

            free(current);

            current = aux;
        }
    }

    free(h->table);

    h->table = NULL;
    h->size = 0;
    h->total_elements = 0;
}

uint8_t getLongestChain (DTCHashTable* h) {
    if (h == NULL || h->total_elements == 0) return 0;

    uint8_t max_chain = 0;

    for (int i = 0; i < h->size; i++) {
        uint8_t current_chain = 0;
        HashNode* aux = h->table[i];

        while (aux != NULL) {
            current_chain++;
            aux = aux->next;
        }

        if (current_chain > max_chain) max_chain = current_chain;
    }

    return max_chain;
}

uint8_t getTotalCollisions (DTCHashTable* h) {
    if (h == NULL || h->total_elements == 0) return 0;

    uint8_t total_collisions = 0;

    for (int i = 0; i < h->size; i++) {
        int current_collisions = -1;
        HashNode* aux = h->table[i];

        while (aux != NULL) {
            current_collisions++;
            aux = aux->next;
        }
        
        if (current_collisions == -1) continue;
        else total_collisions += current_collisions;
    }

    return total_collisions;
}

bool getRawCodeByOBD2 (DTCHashTable* h, const char* obd_code, uint16_t* out_raw) {
    if (h == NULL || h->total_elements == 0) return false;

    for (int i = 0; i < h->size; i++) {
        HashNode* aux = h->table[i];

        while (aux != NULL) {
            if (strcmp(aux->alert.obd2_code, obd_code) == 0) {
                *out_raw = aux->alert.raw_code;
                return true;
            } else aux = aux->next;
        }
    }

    return false;
}