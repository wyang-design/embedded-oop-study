
#include "uart_status.h"
#include "hal.h"

#include <stdio.h>
#include <stdlib.h>

static void uart_publish(status_sink_t *self, float temp, float target,
                         bool heating, bool fault) {
  (void)self; // Unused parameter
  char buffer[100] = {'\0'};

  snprintf(buffer, sizeof(buffer),
           "Temp: %.2f, Target: %.2f, Heating: %s, Fault: %s", temp, target,
           heating ? "Yes" : "No", fault ? "Yes" : "No");
  uart_print(buffer);
}

static const status_sink_ops_t uart_status_ops = {
    .publish = uart_publish,
};

void uart_status_init(uart_status_t *self) {
  self->base.ops = &uart_status_ops;
}
