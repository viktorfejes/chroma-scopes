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

#ifndef ABS
#define ABS(x) ((x) < 0 ? -(x) : (x))
#endif

#define HAS_BIT(x, bit)    (((x) & (bit)) != 0)
#define SET_BIT(x, bit)    ((x) |= (bit))
#define CLEAR_BIT(x, bit)  ((x) &= ~(bit))

#ifndef ARRAY_LENGTH
#define ARRAY_LENGTH(x) (sizeof(x) / sizeof((x)[0]))
#endif
