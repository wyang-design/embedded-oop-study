

#include "hal.h"
#include "lm75_i2c.h"
#include "gpio_heater.h"
#include "uart_status.h"
#include "thermostat.h"
#include <stdio.h>
#include <stdlib.h>



int main(void)
{
    hal_init();
   
    thermostat_config_t config = {
        .target_c = 50.0f,
        .hysteresis_c = 2.0f,
        .max_c = 80.0f,
        .period_ms = 100
    };

    lm75_t sensor;
    lm75_init(&sensor, 0x48); // Initialize LM75 sensor at I2C address 0x48

    gpio_heater_t heater;
    gpio_heater_init(&heater, 5); // Initialize GPIO heater on pin 17

    uart_status_t status;
    uart_status_init(&status); // Initialize UART status sink

    thermostat_t thermostat;
    thermostat_init(&thermostat, (temp_sensor_t *)&sensor, (heater_t *)&heater, (status_sink_t *)&status, &config);
    while(1)
    {
        thermostat_step(&thermostat);
        delay_ms(config.period_ms); // Delay for 1 second before next reading
    }
}