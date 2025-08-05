#include "platform.h"

#include "logger.h"
#include "macros.h"
#include <assert.h>

#define WIN32_LEAN_MEAN
#include <windows.h>

struct platform_internal_state {
    HINSTANCE h_instance;
};

enum platform_window_state {
    WINDOW_OPEN = 1 << 0,
    WINDOW_VISIBLE = 1 << 1,
    WINDOW_FOCUSED = 1 << 2,
    WINDOW_MINIMIZED = 1 << 3,
    WINDOW_RESIZED = 1 << 4,
    WINDOW_DPI_DIRTY = 1 << 5,
};

struct platform_window {
    HWND hwnd;
    uint32_t x;
    uint32_t y;
    uint16_t width;
    uint16_t height;
    uint32_t flags;
    uint32_t state;
};

#define DEFAULT_WIN_CLASS_NAME L"DefaultWinClass"

// Redefine these macros from some windows headers so I don't need to include them...
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

static bool register_window_class(HINSTANCE h_instance);
static LRESULT CALLBACK winproc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param);

bool platform_initialize(size_t *memory_requirement, platform_state_t *state) {
    *memory_requirement = sizeof(platform_state_t) +
                          sizeof(struct platform_internal_state);

    if (!state) {
        return true;
    }

    struct platform_internal_state *internal_state = (struct platform_internal_state *)(state + 1);

    internal_state->h_instance = GetModuleHandle(NULL);
    if (!internal_state->h_instance) {
        return false;
    }

    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
    }

    if (!register_window_class(internal_state->h_instance)) {
        LOG("Failed to register necessary window class with Windows");
        return false;
    }

    // Store seconds per tick
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    state->tick = 1.0 / (double)frequency.QuadPart;

    return true;
}

void platform_terminate(platform_state_t *state) {
    if (state) {
        struct platform_internal_state *internal_state = state->internal_state;
        UnregisterClassW(DEFAULT_WIN_CLASS_NAME, internal_state->h_instance);
    }
}

bool platform_process_messages(void) {
    MSG message;
    while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return true;
}

void platform_sleep(uint64_t ms) {
    // Timer resolution of 1ms
    const UINT timer_resolution = 1;
    timeBeginPeriod(timer_resolution);

    LARGE_INTEGER frequency, start, now;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);

    // Calculate the target counter value
    int64_t target_ticks = start.QuadPart + (ms * frequency.QuadPart) / 1000;

    // Coarse sleep: if ms is greater than the timer resolution,
    // sleep for nearly the full amount.
    if (ms > timer_resolution) {
        Sleep((DWORD)(ms - timer_resolution));
    }

    // Busy-wait until the target time is reached
    while (1) {
        QueryPerformanceCounter(&now);
        if (now.QuadPart >= target_ticks) {
            break;
        }
        // Hint to the CPU we're in a spin-wait loop
        YieldProcessor();
    }

    // Restore the previous timer resolution
    timeEndPeriod(timer_resolution);
}

double platform_get_seconds(platform_state_t *state) {
    LARGE_INTEGER now_time;
    QueryPerformanceCounter(&now_time);
    return (double)now_time.QuadPart * state->tick;
}

bool platform_create_window(platform_state_t *state, platform_window_t *window, const platform_window_desc_t *desc) {
    assert(state);
    assert(window);
    assert(desc);

    DWORD style = (desc->flags & PLATFORM_WINDOW_FLAG_BORDERLESS) ? WS_POPUP : WS_OVERLAPPEDWINDOW;
    DWORD ex_style = (desc->flags & PLATFORM_WINDOW_FLAG_NO_ICON) ? WS_EX_TOOLWINDOW : WS_EX_APPWINDOW;
    if (desc->flags & PLATFORM_WINDOW_FLAG_ON_TOP) ex_style |= WS_EX_TOPMOST;

    int32_t x = desc->x;
    int32_t y = desc->y;
    uint32_t width = desc->width;
    uint32_t height = desc->height;

    if (desc->flags & PLATFORM_WINDOW_FLAG_COVER) {
        x = GetSystemMetrics(SM_XVIRTUALSCREEN);
        y = GetSystemMetrics(SM_YVIRTUALSCREEN);
        width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    } else {
        // Adjust window size to account for decoration
        RECT wr = {.left = 0, .top = 0, .right = (LONG)width, .bottom = (LONG)height};
        AdjustWindowRect(&wr, style, FALSE);
        width = wr.right - wr.left;
        height = wr.bottom - wr.top;
    }

    // Convert UTF-8 to widechar
    wchar_t title_wide[256];
    MultiByteToWideChar(CP_UTF8, 0, desc->title, -1, title_wide, sizeof(title_wide) / sizeof(wchar_t));

    // Create the window itself
    window->hwnd = CreateWindowExW(
        ex_style,
        DEFAULT_WIN_CLASS_NAME,
        title_wide,
        style,
        x, y,
        width, height,
        desc->parent->hwnd,
        NULL,
        ((struct platform_internal_state *)state->internal_state)->h_instance,
        window);

    if (!window->hwnd) {
        LOG("Window creation failed.");
        return false;
    }

    window->x = x;
    window->y = y;
    window->width = width;
    window->height = height;
    window->flags = desc->flags;

    window->state = WINDOW_OPEN;

    if (!(desc->flags & PLATFORM_WINDOW_FLAG_HIDDEN)) {
        window->state |= WINDOW_VISIBLE | WINDOW_FOCUSED;
        platform_show_window(window);
    }

    return true;
}

void platform_destroy_window(platform_state_t *state, platform_window_t *window) {
    assert(window);
    UNUSED(state);

    if (window->hwnd) {
        DestroyWindow(window->hwnd);
    }

    window->hwnd = NULL;
}

bool platform_window_should_close(platform_window_t *window) {
    assert(window);
    return !(window->state & WINDOW_OPEN);
}

void platform_show_window(platform_window_t *window) {
    if (window) {
        SET_BIT(window->state, WINDOW_VISIBLE);
        ShowWindow(window->hwnd, SW_SHOW);
    }
}

void platform_hide_window(platform_window_t *window) {
    if (window) {
        CLEAR_BIT(window->state, WINDOW_VISIBLE);
        ShowWindow(window->hwnd, SW_HIDE);
    }
}

void platform_close_window(platform_window_t *window) {
    if (window) {
        CLEAR_BIT(window->state, WINDOW_OPEN);
        PostMessage(window->hwnd, WM_CLOSE, 0, 0);
    }
}

static bool register_window_class(HINSTANCE h_instance) {
    WNDCLASSEXW wc = {
        .cbSize = sizeof(WNDCLASSEXW),
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = winproc,
        .hInstance = h_instance,
        .hIcon = LoadIcon(NULL, IDI_APPLICATION),
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .lpszClassName = DEFAULT_WIN_CLASS_NAME,
    };

    if (!RegisterClassExW(&wc)) {
        LOG("Couldn't register default window class");
        return false;
    }

    return true;
}

static LRESULT CALLBACK winproc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
    platform_window_t *window = (platform_window_t *)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW *create_struct = (CREATESTRUCTW *)l_param;
            platform_window_t *win = (platform_window_t *)create_struct->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)win);
            return 0;
        }

        default:
            return DefWindowProcW(hwnd, msg, w_param, l_param);
    }
}
