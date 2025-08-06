#pragma once

#include <stdbool.h>
#include <stdint.h>

struct platform_internal_state;
typedef struct platform_window platform_window_t;

typedef enum platform_window_flag {
    PLATFORM_WINDOW_FLAG_NONE = 0,
    PLATFORM_WINDOW_FLAG_HIDDEN = 1 << 0,
    PLATFORM_WINDOW_FLAG_BORDERLESS = 1 << 1,
    PLATFORM_WINDOW_FLAG_NO_ICON = 1 << 2,
    PLATFORM_WINDOW_FLAG_ON_TOP = 1 << 3,
    PLATFORM_WINDOW_FLAG_COVER = 1 << 4,
} platform_window_flag_t;

typedef struct platform_window_desc {
    const char *title;
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    platform_window_t *parent;
    uint32_t flags;
} platform_window_desc_t;

typedef struct platform_state {
    struct platform_internal_state *internal_state;

    double tick;
} platform_state_t;

bool platform_initialize(size_t *memory_requirement, platform_state_t *state);
void platform_terminate(platform_state_t *state);

bool platform_process_messages(void);
void platform_sleep(uint64_t ms);
double platform_get_seconds(platform_state_t *state);

bool platform_create_window(platform_state_t *state, platform_window_t *window, const platform_window_desc_t *desc);
void platform_destroy_window(platform_state_t *state, platform_window_t *window);
bool platform_window_should_close(platform_window_t *window);
void platform_show_window(platform_window_t *window);
void platform_hide_window(platform_window_t *window);
void platform_close_window(platform_window_t *window);
