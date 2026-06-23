#ifndef HASH_H
#define HASH_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t raw_code;
    char obd2_code[6];
    char description[50];
    uint8_t severity;
} DTCAlert; // Diagnostic Trouble Code (DTC)

typedef struct HashNode {
    DTCAlert alert;
    struct HashNode* next;
} HashNode;

typedef struct {
    HashNode** table; // Vetor dinâmico de ponteiros para os nós
    uint16_t size;
    uint8_t total_elements;
} DTCHashTable;

// Funções básicas
void initDTCHashTable (DTCHashTable* h, uint16_t size);
int calculateHash (DTCHashTable* h, uint16_t raw_code);
bool insertDTC (DTCHashTable* h, DTCAlert alert);
bool removeDTC (DTCHashTable* h, uint16_t raw_code);
DTCAlert* searchDTC (DTCHashTable* h, uint16_t raw_code);
void freeDTCHashTable (DTCHashTable* h);

// Funções adicionais
uint8_t getLongestChain (DTCHashTable* h);
uint8_t getTotalCollisions (DTCHashTable* h);
bool getRawCodeByOBD2 (DTCHashTable* h, const char* obd_code, uint16_t* out_raw);

#endif