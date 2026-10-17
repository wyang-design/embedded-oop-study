

#include "hal.h"



#include <stdlib.h>
#include <unistd.h>

#include <stdio.h>


static float fake_temp = 45.0f;

void hal_init(void)
{
    // Initialize hardware abstraction layer
    printf("[HAL] initialized.\n");
}

bool i2c_read(uint8_t addr, uint8_t reg, uint8_t *data, size_t len)
{
    // Simulate reading from I2C device
    printf("[HAL] I2C read from addr: 0x%02X, reg: 0x%02X, len: %zu\n", addr, reg, len);
    static int tick = 0;

    tick ++;
    fake_temp += ((rand() % 100) -  50) / 100.0f; // Simulate temperature fluctuation
    if(fake_temp < 40.0f) fake_temp = 40.0f;
    if(fake_temp > 90.0f) fake_temp = 90.0f;

    int16_t raw = (int16_t)(fake_temp / 0.125f) << 5; // Convert to raw value

    data[0] = (raw >> 8) & 0xFF;
    data[1] = raw & 0xFF;

    return true;
}


void gpio_set(uint8_t pin, bool value)
{
    // Simulate setting GPIO pin
    printf("[HAL] GPIO set pin: %d to value: %d\n", pin, value);
}


void uart_print(const char *msg)
{
    printf("[HAL] UART print: %s\n", msg);
}

void delay_ms(uint32_t ms)
{
    usleep(ms * 1000); // Convert milliseconds to microseconds
}

