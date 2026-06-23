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