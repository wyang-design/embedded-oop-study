#include "lm75_i2c.h"

#include "hal.h"

static bool lm75_read(temp_sensor_t *self, float *out)
{
    lm75_t *lm75 = (lm75_t *)self;
    uint8_t data[2];
    if (!i2c_read(lm75->addr, 0x00,data, 2))
    {
        return false;
    }
    int16_t temp_raw = (data[0] << 8) | data[1];
    *out = (temp_raw >> 5) * 0.125f; // Convert to Celsius
    return true;
}

static temp_sensor_ops_t lm75_ops = {
    .read_celsius = lm75_read,
};



void lm75_init(lm75_t *self, uint8_t addr)
{
    self->base.ops = &lm75_ops;
    self->addr = addr;
}

