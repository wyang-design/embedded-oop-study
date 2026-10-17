

#include "hal.h"
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

    thermostat_t thermostat;
    thermostat_init(&thermostat, &config);

    bool last_heating = false;
    while(1)
    {
        uint8_t data[2] = {0};
        i2c_read(0x48, 0x00, data, sizeof(data));
        int16_t raw_temp = (int16_t)((data[0] << 8) | data[1]);
        float temperature = (raw_temp >> 5) * 0.125f; // Convert raw value to

        

        thermostat_step(&thermostat, temperature);

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