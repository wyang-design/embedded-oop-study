#pragma once

#include "ports/temp_sensor.h"

#include <stdint.h>


typedef struct 
{
    temp_sensor_t base;
    uint8_t addr;
} lm75_t;


void lm75_init(lm75_t *self, uint8_t addr);