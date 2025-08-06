#pragma once

#include <stdbool.h>

typedef struct rhi {
    bool (*initialize)(struct rhi *rhi);
    void (*shutdown)(struct rhi *rhi);
} rhi_t;
