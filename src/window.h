#pragma once

#define WIN32_LEAN_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdbool.h>

typedef struct window {
    uint16_t width;
    uint16_t height;
    uint32_t x;
    uint32_t y;
    HWND hwnd;
    bool should_close;
} window_t;

bool window_create(const TCHAR *title, uint16_t width, uint16_t height, window_t *out_window);
void window_destroy(window_t *window);
void window_proc_messages(void);
bool window_should_close(window_t *window);
void window_set_always_on_top(window_t *window, bool enable);

bool platform_initialize(void);
void platform_sleep(uint64_t ms);
double platform_get_seconds(void);
