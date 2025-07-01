#pragma once

#define UNUSED(x) ((void)(x))

// Need to replicate IID_PPV_ARGS macro from C++
#define IID_PPV_ARGS_C(type, ppType) \
    &IID_##type, (void **)(ppType)
