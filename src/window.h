#pragma once

#include "math.h"

#define WIN32_LEAN_MEAN
#include <windows.h>

#include <stdbool.h>
#include <stdint.h>

struct internal_state;

// typedef struct monitor_info {
//     rect_t bounds;
//     rect_t work_area;
//     void *handle;
//     float dpi_scale;
//     uint16_t refresh_rate;
//     uint8_t bpp;
//     bool is_primary;
// } monitor_info_t;

typedef enum window_flag {
    WINDOW_FLAG_NONE            = 0,
    WINDOW_FLAG_BORDERLESS      = 1 << 0,
    WINDOW_FLAG_NO_TASKBAR_ICON = 1 << 1,
    WINDOW_FLAG_ALWAYS_ON_TOP   = 1 << 2,
    WINDOW_FLAG_TRANSPARENT     = 1 << 3,
    WINDOW_FLAG_MONITOR_SIZE    = 1 << 5,
} window_flag_t;

typedef struct window_create_info {
    const TCHAR *title;
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    window_flag_t flags;
} window_create_info_t;

typedef struct window {
    HWND hwnd;
    uint32_t x;
    uint32_t y;
    uint16_t width;
    uint16_t height;
    rect_t custom_dragbar;
    window_flag_t flags;
    bool should_close;
} window_t;

typedef struct platform_state {
    /** @brief Platform specific internal state */
    HINSTANCE h_instance;

    /** @brief Array of monitor infos */
    // monitor_info_t *monitors;
    /** @brief Count of found monitors (corresponds to monitor_info_t array count) */
    uint8_t monitor_count;
} platform_state_t;

bool platform_initialize(platform_state_t *state);
void platform_terminate(platform_state_t *state);

bool window_create(platform_state_t *state, window_create_info_t *info, window_t *out_window);
bool window_create_overlay(platform_state_t *state, window_t *out_window);
void window_overlay_show(window_t *window);
void window_destroy(window_t *window);
void window_proc_messages(window_t *window);
bool window_should_close(window_t *window);
void window_post_close(window_t *window);
void window_minimize(window_t *window);
void window_maximize_restore(window_t *window);
void window_set_always_on_top(window_t *window, bool enable);
void window_set_custom_dragarea(window_t *window, rect_t area);
window_t *window_get_from_point(int2_t point);
bool window_get_rect(window_t *window, rect_t *rect);
bool window_set_window_pos(window_t *window, int32_t x, int32_t y);
bool window_is_maximized(window_t *window);
int2_t window_client_to_screen(window_t *window, int2_t client_point);

void platform_sleep(uint64_t ms);
double platform_get_seconds(void);
int2_t platform_get_screen_cursor_pos(void);
