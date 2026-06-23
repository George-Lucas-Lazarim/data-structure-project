#include <stdio.h>
#include <stdlib.h>
#include "bloom-filter.h"

void initBloomFilter (BloomFilter* bf, uint16_t size_in_bits, uint8_t num_hashes) {
    if (bf == NULL) {
        printf("\nErro! O ponteiro do bloom filter esta apontando para NULL");
        return;
    } else if (size_in_bits == 0) return;

    uint16_t num_bytes = (size_in_bits + 7) / 8; // +7 para arredondar para cima

    bf->bit_array = (uint8_t*) malloc (num_bytes * sizeof(uint8_t));

    if (bf->bit_array == NULL) return;

    for (int i = 0; i < num_bytes; i++) bf->bit_array[i] = 0;

    bf->size_in_bits = size_in_bits;
    bf->num_hash_functions = num_hashes;
}

void insertBloomFilter (BloomFilter* bf, uint16_t raw_code) {
    if (bf == NULL) {
        printf("\nErro! O bloom filter nao foi inicializado");
        return;
    } else if (bf->bit_array == NULL) return;

    for (int i = 0; i < bf->num_hash_functions; i++) {
        uint16_t bit_index = (raw_code * (i + 1) * 73) % bf->size_in_bits;
        uint16_t byte_index = bit_index / 8;
        uint8_t bit_offset = bit_index % 8;

        bf->bit_array[byte_index] |= (1 << bit_offset); // 
    }
}

bool checkBloomFilter (BloomFilter* bf, uint16_t raw_code) {
    if (bf == NULL) {
        printf("\nErro! O bloom filter nao foi inicializado");
        return false;
    } else if (bf->bit_array == NULL) return false;

    for (int i = 0; i < bf->num_hash_functions; i++) {
        uint16_t bit_index = (raw_code * (i + 1) * 73) % bf->size_in_bits;
        uint16_t byte_index = bit_index / 8;
        uint8_t bit_offset = bit_index % 8;

        if ((bf->bit_array[byte_index] & (1 << bit_offset)) == 0) return false;
    }

    return true;
}

void freeBloomFilter (BloomFilter* bf) {
    if (bf == NULL || bf->bit_array== NULL) return;

    free(bf->bit_array);

    bf->bit_array = NULL;
    bf->num_hash_functions = 0;
    bf->size_in_bits = 0;
}

void clearBloomFilter (BloomFilter* bf) {
    if (bf == NULL || bf->bit_array == NULL) return;

    uint16_t num_bytes = (bf->size_in_bits + 7) / 8;

    for (int i = 0; i < num_bytes; i++) bf->bit_array[i] = 0;
}

float getBloomFilterOccupancy (BloomFilter* bf) {
    if (bf == NULL || bf->bit_array == NULL || bf->size_in_bits == 0) return 0.0;

    float bits_ligados = 0;
    uint16_t num_bytes = (bf->size_in_bits + 7) / 8;

    for (int i = 0; i < num_bytes; i++) {
        // Bytes (8 bits)
        for (int j = 0; j < 8; j++) {
            // Testa cada bit
            if ((bf->bit_array[i] & (1 << j)) != 0) bits_ligados++;
        }
    }

    return bits_ligados / bf->size_in_bits;
}

float estimateFalsePositiveRate (BloomFilter* bf) {
    if (bf == NULL) return 0.0;
    
    return pow(getBloomFilterOccupancy(bf), bf->num_hash_functions); // p^k onde p é a razão ocupado/tamanho e k é o número de hashes
}