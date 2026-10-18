#include "temp_sensor.h"
#include "thermostat.h"
#include <assert.h>
#include <stdio.h>

typedef struct{
    temp_sensor_t base;
    float value;
    bool ok;
}fake_sensor_t;

static bool fake_sensor_read(temp_sensor_t *self, float *value)
{
    fake_sensor_t *sensor = (fake_sensor_t *)self;
    if(sensor->ok){
        *value = sensor->value;
        return true;
    }else{
        return false;
    }
}

static temp_sensor_ops_t fake_sensor_ops = {
    .read_celsius = fake_sensor_read
};

static fake_sensor_t make_fake_sensor(float value, bool ok)
{
    fake_sensor_t sensor = {
        .base.ops = &fake_sensor_ops,
        .value = value,
        .ok = ok
    };
    return sensor;
}


static thermostat_config_t make_cfg(void)
{
    thermostat_config_t cfg = {
        .target_c = 50.0f,
        .hysteresis_c = 2.0f,
        .max_c = 80.0f,
        .period_ms = 100
    };
    return cfg;
}

static void test_over_temp(void)
{
    thermostat_config_t cfg = make_cfg();
    thermostat_t t;

    fake_sensor_t sensor = make_fake_sensor(85.0f, true);

    thermostat_init(&t, &sensor.base, &cfg);

    thermostat_step(&t);

    assert(t.fault == true);
    assert(t.heating == false);

    printf("PASS:over temp\n");
}

static void test_low_temp_heats(void)
{
    thermostat_config_t cfg = make_cfg();
    thermostat_t t;

    fake_sensor_t sensor = make_fake_sensor(47.0f, true);

    thermostat_init(&t, &sensor.base, &cfg);

    thermostat_step(&t);

    assert(t.fault == false);
    assert(t.heating == true);

    printf("PASS:low temp heats\n");
}

static void test_hysteresis(void)
{
    thermostat_config_t cfg = make_cfg();
    thermostat_t t;
    fake_sensor_t sensor = make_fake_sensor(47.0f, true);

    thermostat_init(&t, &sensor.base, &cfg);

    thermostat_step(&t);

    // Step to low temp to turn on heating
    sensor.value = 47.0f;
    thermostat_step(&t);
    assert(t.heating == true);

    // Step to a temp within hysteresis range, heating should remain on
    sensor.value = 49.0f;
    thermostat_step(&t);
    assert(t.heating == true);

    // Step to a temp above target + hysteresis, heating should turn off
    sensor.value = 53.0f;
    thermostat_step(&t);
    assert(t.heating == false);

    printf("PASS:hysteresis\n");
}

int main(void)
{
    test_over_temp();
    test_low_temp_heats();
    test_hysteresis();

    printf("All tests passed!\n");
    return 0;
}

