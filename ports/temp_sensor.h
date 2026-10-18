#pragma once


#include <stdbool.h>

typedef struct temp_sensor temp_sensor_t;

typedef struct 
{
    bool (*read_celsius)(temp_sensor_t *self, float *out);
}temp_sensor_ops_t;


struct temp_sensor
{
    const temp_sensor_ops_t *ops;
};


static inline bool temp_sensor_read(temp_sensor_t *self, float *out)
{
    return self->ops->read_celsius(self, out);
}

