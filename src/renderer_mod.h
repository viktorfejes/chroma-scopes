#pragma once

#include "vri/vri.h"

#include <stdbool.h>
#include <stddef.h>

struct platform_dynlib;

typedef struct renderer_state {
    vri_device_t *device;
    vri_queue_t *queue;
} renderer_state_t;

bool renderer_initialize(size_t *memory_requirement, renderer_state_t *state, vri_api_t api);
