
#include "thermostat.h"

void thermostat_init(thermostat_t *self, temp_sensor_t *sensor, heater_t *heater, status_sink_t *status, const thermostat_config_t *config)
{
    self->config = *config;
    self->sensor = sensor;
    self->heater = heater;
    self->status = status;
    self->heating = false;
    self->fault = false;
    self->last_temp = 0.0f;
}
void thermostat_step(thermostat_t *self)
{
    float current_temp = 0.0f;
    
    if(!temp_sensor_read(self->sensor, &current_temp))
    {
        self->fault = true;
        self->heating = false;
        status_sink_publish(self->status, current_temp, self->config.target_c, self->heating, self->fault);
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

    if(self->heating && !self->fault)
    {
        heater_set_power(self->heater, 100.0f);
    }
    else
    {
        heater_set_power(self->heater, 0.0f);
    }
    status_sink_publish(self->status, current_temp, self->config.target_c, self->heating, self->fault);
}