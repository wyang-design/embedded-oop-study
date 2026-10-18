#include "../domain/thermostat.h"
#include <assert.h>
#include <stdio.h>


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

    thermostat_init(&t, &cfg);

    thermostat_step(&t, 85.0f);

    assert(t.fault == true);
    assert(t.heating == false);

    printf("PASS:over temp\n");
}

static void test_low_temp_heats(void)
{
    thermostat_config_t cfg = make_cfg();
    thermostat_t t;

    thermostat_init(&t, &cfg);

    thermostat_step(&t, 47.0f);

    assert(t.fault == false);
    assert(t.heating == true);

    printf("PASS:low temp heats\n");
}

static void test_hysteresis(void)
{
    thermostat_config_t cfg = make_cfg();
    thermostat_t t;

    thermostat_init(&t, &cfg);

    // Step to low temp to turn on heating
    thermostat_step(&t, 47.0f);
    assert(t.heating == true);

    // Step to a temp within hysteresis range, heating should remain on
    thermostat_step(&t, 49.0f);
    assert(t.heating == true);

    // Step to a temp above target + hysteresis, heating should turn off
    thermostat_step(&t, 53.0f);
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

