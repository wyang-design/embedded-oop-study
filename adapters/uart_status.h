#pragma once

#include "status_sink.h"


typedef struct {
    status_sink_t base;
} uart_status_t;

void uart_status_init(uart_status_t *self);