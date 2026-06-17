#include "synthetic-data.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

static uint32_t current_time_ms = 0;
static float current_rpm = 900.0f;
static float current_temp = 30.0f;
static float current_boost = -0.6f;

// Variáveis de Estresse e Dano
static float engine_stress = 0.0f;
static bool turbo_blown = true;
static bool oil_pump_working = true;
static bool engine_working = true;

static float randomFloat (float min, float max) {
    float scale = rand() / (float) RAND_MAX; // 0 ~ 1
    return min + scale * (max - min);
}

void initEngineSimulation (void) {
    srand(time(NULL));
    current_time_ms = 0;
    current_rpm = 900.0f;
    current_temp = 30.0f;
    current_boost = -0.6f;

    engine_stress = 0.0f;
    turbo_blown = true;
    oil_pump_working = true;
    engine_working = true;
}

EngineSensorsData getNextSensorsDataBlock (void) {
    EngineSensorsData data;

    current_time_ms += 12;
    data.time_ms = current_time_ms;

    if (!engine_working) {
        if (current_rpm > 0) current_rpm -= 500.0f;
        else current_rpm = 0.0f;

        data.tps = 0;
        data.rpm = (uint16_t) current_rpm;
        data.oil_pressure = 0.0f;
        data.turbo_pressure = 0.0f;
        data.temperature = current_temp;

        return data;
    }

    // TPS
    int tps_chance = rand() % 100;
    
    if (tps_chance > 75) data.tps = (uint8_t) randomFloat(30.0f, 100.0f); // 25%
    else if (tps_chance > 30) data.tps = (uint8_t) randomFloat(0.0f, 20.0f); // 45%
    else data.tps = 0; // 30%

    // Turbocompressor
    if (turbo_blown) {
        if (data.tps > 20 && current_rpm > 2500.0f) {
            float spool_factor = (current_rpm - 2500.0f) / 4500.0f;
            float tps_factor = data.tps / 100.0f;

            current_boost += 0.37f;
            float max_target_boost = spool_factor * tps_factor * 1.5f; // Pico de pressão

            if (current_boost > max_target_boost) current_boost = max_target_boost;
        } else current_boost = randomFloat(-0.6f, -0.4f);
    } else current_boost = randomFloat(-0.6f, -0.1f);

    data.turbo_pressure = current_boost + randomFloat(-0.02f, 0.02f);

    // RPM
    if (data.tps > 0) {
        float boost_assist = 1.0f;

        if (current_boost > 0) boost_assist = current_boost * 2.0f;
        if (!turbo_blown) boost_assist = 0.7f; // Se a turbina quebrou, o giro sobe mais devagar

        current_rpm += data.tps * 3.5f * boost_assist;
    } else current_rpm -= 180.0f; // Freio motor

    if (current_rpm > 7200.0f) current_rpm = 7200.0f; // Corte
    else if (current_rpm < 900.0f) current_rpm = 900.0f + randomFloat(-15.0, 15.0); // Leve oscilação na marcha lenta

    data.rpm = (uint16_t) current_rpm;

    // Pressão do Óleo
    if (oil_pump_working) {
        data.oil_pressure = 1.0f + (current_rpm / 1800.0f) + randomFloat(-0.1f, 0.1f);

        if (data.oil_pressure > 5.0f) data.oil_pressure = 5.0f; // Alívio da bomba
        if (!turbo_blown) data.oil_pressure -= 0.7f;
        if (data.oil_pressure < 0) data.oil_pressure = 0.0f;
    } else data.oil_pressure = 0.0f;

    // Temperatura
    if (current_rpm > 4500.0f || current_boost > 0.5f) current_temp += 0.15f;
    if (data.oil_pressure < 0.5f && current_rpm > 2000.0f) current_temp += 0.5f;

    if (current_temp > 90.0f && oil_pump_working) current_temp -= 0.35f; // Ventoinha liga
    else if (current_temp < 85.0f) current_temp += 0.2f; // Aquece até a temperatura de trabalho

    data.temperature = current_temp;

    // Fadiga e Quebra
    if (current_rpm > 6800.0f) engine_stress += 0.3f;
    if (current_temp > 105.0f) engine_stress += 2.0f;
    if (current_boost > 1.3f) engine_stress += 1.5f;

    if (current_rpm < 4000.0f && current_temp < 95.0f && engine_stress >= 0.3f) engine_stress -= 0.3f;

    if (engine_stress > 250.0f) {
        float random_number = randomFloat(0.0f, 10000.0f);

        if (random_number < (engine_stress * 0.25f) && turbo_blown) turbo_blown = false; // Turbina quebrou
        else if (random_number < (engine_stress * 0.05f) && oil_pump_working) oil_pump_working = false;
    }

    if (data.oil_pressure < 0.5f && current_rpm > 3000.0f) {
        engine_stress += 5.0f;
        if (engine_stress > 500.0f) engine_working = false;
    }

    return data;
}