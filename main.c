

#include "hal.h"
#include "lm75_i2c.h"
#include "thermostat.h"
#include <stdio.h>
#include <stdlib.h>


#define HEATER_PIN 5



static void apply_heater(bool on)
{
    gpio_set(HEATER_PIN, on);
}



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



    thermostat_t thermostat;
    thermostat_init(&thermostat, (temp_sensor_t *)&sensor, &config);

    bool last_heating = false;
    while(1)
    {

        thermostat_step(&thermostat);

        if(thermostat.heating != last_heating)
        {
            apply_heater(thermostat.heating);
            last_heating = thermostat.heating;
        }
        char msg[64];
        snprintf(msg, sizeof(msg), "Current Temp: %.2f C, Heating: %s, Fault: %s", 
                 thermostat.last_temp, 
                 thermostat.heating ? "ON" : "OFF", 
                 thermostat.fault ? "YES" : "NO");
        uart_print(msg);
        delay_ms(config.period_ms); // Delay for 1 second before next reading
    }
}