#pragma once

#include <stdbool.h>

typedef struct status_sink status_sink_t;

typedef struct {
  void (*publish)(status_sink_t *self, float temp, float target, bool heating,
                  bool fault);
} status_sink_ops_t;

struct status_sink {
  const status_sink_ops_t *ops;
};

static inline void status_sink_publish(status_sink_t *self, float temp,
                                       float target, bool heating, bool fault) {
  self->ops->publish(self, temp, target, heating, fault);
}
