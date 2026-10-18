#include "gpio_heater.h"
#include "hal.h"

static void gpio_heater_set_power(heater_t *self, float percent) {
  gpio_heater_t *gpio_heater = (gpio_heater_t *)self;
  bool on = percent > 0.0f;
  gpio_set(gpio_heater->pin, on);
}

static const heater_ops_t gpio_heater_ops = {
    .set_power = gpio_heater_set_power,
};

void gpio_heater_init(gpio_heater_t *self, uint8_t pin) {
  self->base.ops = &gpio_heater_ops;
  self->pin = pin;
}
