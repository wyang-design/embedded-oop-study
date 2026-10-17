

#include "hal.h"

#include <stdio.h>
#include <stdlib.h>


#define HEATER_PIN 5
#define TEMP_TARGET 50.0f
#define HYST_C 2.0f
#define MAX_C 80.0f




int main(void)
{
    hal_init();
    bool heating = false;


    while(1)
    {
        uint8_t data[2] = {0};
        i2c_read(0x48, 0x00, data, sizeof(data));
        int16_t raw_temp = (int16_t)((data[0] << 8) | data[1]);
        float temperature = (raw_temp >> 5) * 0.125f; // Convert raw value to

        char msg[64];

        if(temperature >= MAX_C)
        {
            gpio_set(HEATER_PIN, false);
            heating = false;
            snprintf(msg,sizeof(msg), "Temperature %.2f°C exceeds max limit. Heater turned OFF.", temperature);
        }
        else if(!heating && temperature < (TEMP_TARGET - HYST_C))
        {
            gpio_set(HEATER_PIN, true);
            heating = true;
            snprintf(msg,sizeof(msg), "Temperature %.2f°C below target. Heater turned ON.", temperature);
        }
        else if(heating && temperature > (TEMP_TARGET + HYST_C))
        {
            gpio_set(HEATER_PIN, false);
            heating = false;
            snprintf(msg,sizeof(msg), "Temperature %.2f°C above target. Heater turned OFF.", temperature);
        }
        else
        {
            snprintf(msg,sizeof(msg), "Temperature %.2f°C. Heater state unchanged.", temperature);
        }

        uart_print(msg);
        delay_ms(1000); // Delay for 1 second before next reading
    }
}