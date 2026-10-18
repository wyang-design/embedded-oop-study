#pragma once

typedef struct heater heater_t;

typedef struct {
  void (*set_power)(heater_t *self, float percent);
} heater_ops_t;

struct heater {
  const heater_ops_t *ops;
};

static inline void heater_set_power(heater_t *self, float percent) {
  self->ops->set_power(self, percent);
}
