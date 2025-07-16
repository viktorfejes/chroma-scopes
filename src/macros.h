#pragma once

#define UNUSED(x) ((void)(x))

// Need to replicate IID_PPV_ARGS macro from C++
#define IID_PPV_ARGS_C(type, ppType) \
    &IID_##type, (void **)(ppType)

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef CLAMP
#define CLAMP(value, min, max) MIN((max), MAX((min), (value)))
#endif
