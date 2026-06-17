#include "synthetic-data.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static uint32_t current_time_ms = 0;
static float current_rpm = 900.0f;
static float current_temp = 30.0f;
static float current_boost = -0.6f;

static float randomFloat (float min, float max) {
    float scale = rand() / (float) RAND_MAX;    // 0 ~ 1
    return min + scale * (max - min);
}

void initEngineSimulation (void) {
    srand(time(NULL));
    current_time_ms = 0;
    current_rpm = 900.0f;
    current_temp = 30.0f;
    current_boost = -0.6f;
}

EngineSensorsData getNextSensorsDataBlock (void) {
    EngineSensorsData data;

    current_time_ms += 12;
    data.time_ms = current_time_ms;

    // TPS
    int tps_chance = rand() % 100; // 20% de chance de pisar fundo, 30% de aceleração parcial, 50% solto

    if (tps_chance > 80) {
        data.tps = (uint8_t) randomFloat(80.0, 100.0);
    } else if (tps_chance > 50) {
        data.tps = (uint8_t) randomFloat(10.0, 50.0);
    } else {
        data.tps = 0;
    }

    // Turbocompressor
    if (data.tps > 20 && current_rpm > 2500.0f) {
        float spool_factor = (current_rpm - 2500.0f) / 4500.0f;
        float tps_factor = data.tps / 100.0f;

        current_boost += 0.2f;
        float max_target_boost = spool_factor * tps_factor * 1.2f; // Pico de pressão

        if (current_boost > max_target_boost) current_boost = max_target_boost;
    } else {
        current_boost = randomFloat(-0.6f, -0.4f);
    }
    data.turbo_pressure = current_boost + randomFloat(-0.02f, 0.02f);

    // RPM
    if (data.tps > 0) {
        current_rpm += data.tps * 3.5f;
    } else {
        current_rpm -= 180.0f; // Freio motor
    }

    if (current_rpm > 7200.0f) {
        current_rpm = 7200.0f; // Corte
    } else if (current_rpm < 900.0f) {
        current_rpm = 900.0f + randomFloat(-15.0, 15.0); // Leve oscilação na marcha lenta
    }
    data.rpm = (uint16_t) current_rpm;

    // Pressão do Óleo
    data.oil_pressure = 1.0f + (current_rpm / 1800.0f) + randomFloat(-0.1f, 0.1f);
    if (data.oil_pressure > 5.0f) data.oil_pressure = 5.0f; // Alívio da bomba

    if (current_rpm > 4500.0f) {
        current_temp += 0.1f;
    } else {
        current_temp -= 0.05f;
    }

    // Temperatura do Líquido de Arrefecimento
    if (current_temp > 95.0f) {
        current_temp -= 0.3f; // Ventoinha liga
    } else if (current_temp < 85.0f) {
        current_temp += 0.1f; // Aquece até a temperatura de trabalho
    }
    data.temperature = current_temp;

    return data;
}