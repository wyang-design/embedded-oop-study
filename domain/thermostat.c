
#include "thermostat.h"

void thermostat_init(thermostat_t *self, temp_sensor_t *sensor, const thermostat_config_t *config)
{
    self->config = *config;
    self->sensor = sensor;
    self->heating = false;
    self->fault = false;
    self->last_temp = 0.0f;
}
void thermostat_step(thermostat_t *self)
{
    float current_temp;
    
    if(!temp_sensor_read(self->sensor, &current_temp))
    {
        self->fault = true;
        self->heating = false;
        return;
    }
    self->last_temp = current_temp;



    if (current_temp >= self->config.max_c)
    {
        self->heating = false;
        self->fault = true;
    }
    else if (!self->heating && current_temp < (self->config.target_c - self->config.hysteresis_c))
    {
        self->heating = true;
        self->fault = false;
    }
    else if (self->heating && current_temp > (self->config.target_c + self->config.hysteresis_c))
    {
        self->heating = false;
        self->fault = false;
    }
    else
    {
        self->fault = false;
    }
}