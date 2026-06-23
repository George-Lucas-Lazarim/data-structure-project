#ifndef BLOOM_FILTER_H
#define BLOOM_FILTER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t* bit_array;
    uint16_t size_in_bits;
    uint8_t num_hash_functions;
} BloomFilter;

// Funções básicas
void initBloomFilter (BloomFilter* bf, uint16_t size_in_bits, uint8_t num_hashes);
void insertBloomFilter (BloomFilter* bf, uint16_t raw_code);
bool checkBloomFilter (BloomFilter* bf, uint16_t raw_code);
void freeBloomFilter (BloomFilter* bf);

// Funções adicionais
void clearBloomFilter (BloomFilter* bf);
float getBloomFilterOccupancy (BloomFilter* bf);
float estimateFalsePositiveRate (BloomFilter* bf);

#endif