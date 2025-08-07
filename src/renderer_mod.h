#pragma once

#include "rhi.h"

#include <stdbool.h>
#include <stddef.h>

struct platform_dynlib;

typedef struct renderer_state {
    rhi_device_t *device;
    rhi_queue_t *queue;

    rhi_api_t rhi;
} renderer_state_t;

bool renderer_initialize(size_t *memory_requirement, renderer_state_t *state, struct platform_dynlib *backend_plugin);
