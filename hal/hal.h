#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>


void hal_init(void);
bool i2c_read(uint8_t addr, uint8_t reg, uint8_t *data, size_t len);
void gpio_set(uint8_t pin, bool value);
void uart_print(const char *format);
void delay_ms(uint32_t ms);