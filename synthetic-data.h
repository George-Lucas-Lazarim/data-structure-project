#ifndef SYNTHETIC_DATA_H
#define SYNTHETIC_DATA_H

#include <stdint.h> // Controle absoluto sobre o consumo de memória

typedef struct {
    uint32_t time_ms;
    uint16_t rpm;
    uint8_t tps;
    float oil_pressure;
    float temperature;
    float turbo_pressure;
} EngineSensorsData;

void initEngineSimulation (void);
EngineSensorsData getNextSensorsDataBlock (void);

#endif