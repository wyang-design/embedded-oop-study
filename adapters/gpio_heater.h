#pragma once

#include <stdint.h>
#include "heater.h"



typedef struct {
    heater_t base;

    uint8_t pin;
} gpio_heater_t;

void gpio_heater_init(gpio_heater_t *self, uint8_t pin);