#pragma once


#include <stdint.h>
#include <stdbool.h>


typedef struct _thermostat_config_t
{
    float target_c;
    float hysteresis_c;
    float max_c;
    uint32_t period_ms;

}thermostat_config_t;

typedef struct _thermostat_t
{
    thermostat_config_t config;
    bool heating;
    bool fault;
    float last_temp;
}thermostat_t;


void thermostat_init(thermostat_t *self, thermostat_config_t *config);
void thermostat_step(thermostat_t *self, float current_temp);

