#include "window.h"

#include "input.h"
#include "logger.h"
#include "macros.h"

#include <assert.h>
#include <shellscalingapi.h>
#include <windows.h>
#include <wingdi.h>

#define DEFAULT_WIN_CLASS_NAME L"DefaultWinClassName"
#define OVERLAY_WIN_CLASS_NAME L"OverlayWinClassName"

#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

static HBITMAP overlay_screenshot = NULL;

static double seconds_per_tick; // 1.0 / frequency (precomputed)

static keycode_t vk_to_keycode(WPARAM w_param);
static LRESULT CALLBACK winproc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param);
static LRESULT CALLBACK winproc_overlay(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param);
static BOOL window_resizing(window_t *window, WPARAM w_param, LPARAM l_param);
static void capture_screen_for_overlay(void);

bool platform_initialize(platform_state_t *state) {
    state->h_instance = GetModuleHandle(NULL);

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Register main window class
    {
        WNDCLASSEXW wc = {
            .cbSize = sizeof(WNDCLASSEXW),
            .style = CS_HREDRAW | CS_VREDRAW,
            .lpfnWndProc = winproc,
            .hInstance = state->h_instance,
            .hIcon = LoadIcon(NULL, IDI_APPLICATION),
            .hCursor = LoadCursor(NULL, IDC_ARROW),
            .lpszClassName = DEFAULT_WIN_CLASS_NAME,
        };

        if (!RegisterClassExW(&wc)) {
            LOG("Couldn't register default window class");
            return false;
        }
    }

    // Register overlay window class
    {
        WNDCLASSEXW wc = {
            .cbSize = sizeof(WNDCLASSEXW),
            .style = CS_HREDRAW | CS_VREDRAW,
            .lpfnWndProc = winproc_overlay,
            .hInstance = state->h_instance,
            .hCursor = LoadCursor(NULL, IDC_CROSS),
            .lpszClassName = OVERLAY_WIN_CLASS_NAME,
            .hbrBackground = NULL,
        };

        if (!RegisterClassExW(&wc)) {
            LOG("Couldn't register overlay window class");
            return false;
        }
    }

    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    seconds_per_tick = 1.0 / (double)frequency.QuadPart;

    return true;
}

void platform_terminate(platform_state_t *state) {
    UNUSED(state);
}

bool window_create(platform_state_t *state, window_create_info_t *info, window_t *out_window) {
    assert(state);
    assert(info);
    assert(out_window && "Out window cannot be NULL");

    DWORD w_style = (info->flags & WINDOW_FLAG_BORDERLESS) ? WS_POPUP : WS_OVERLAPPEDWINDOW;
    DWORD w_ex_style = (info->flags & WINDOW_FLAG_NO_TASKBAR_ICON) ? WS_EX_TOOLWINDOW : WS_EX_APPWINDOW;

    if (info->flags & WINDOW_FLAG_ALWAYS_ON_TOP) w_ex_style |= WS_EX_TOPMOST;
    if (info->flags & WINDOW_FLAG_TRANSPARENT) w_ex_style |= WS_EX_LAYERED;

    int32_t x = info->x;
    int32_t y = info->y;
    uint32_t w = info->width;
    uint32_t h = info->height;

    if (info->flags & WINDOW_FLAG_MONITOR_SIZE) {
        x = GetSystemMetrics(SM_XVIRTUALSCREEN);
        y = GetSystemMetrics(SM_YVIRTUALSCREEN);
        w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    } else {
        // Adjust window size to account for decoration
        RECT wr = {.left = 0, .top = 0, .right = (LONG)w, .bottom = (LONG)h};
        AdjustWindowRect(&wr, w_style, FALSE);
        w = wr.right - wr.left;
        h = wr.bottom - wr.top;
    }

    // Convert title to wide char from char
    wchar_t title_wide[256];
    MultiByteToWideChar(CP_UTF8, 0, info->title, -1, title_wide, sizeof(title_wide) / sizeof(wchar_t));

    // Create the window itself
    out_window->hwnd = CreateWindowExW(
        w_ex_style,
        DEFAULT_WIN_CLASS_NAME,
        title_wide,
        w_style,
        x, y,
        w, h,
        NULL,
        NULL,
        state->h_instance,
        out_window);

    if (!out_window->hwnd) {
        LOG("Window creation failed.");
        return false;
    }

    // For transparent windows we set the layered attributes
    if (info->flags & WINDOW_FLAG_TRANSPARENT) {
        SetLayeredWindowAttributes(out_window->hwnd, 0, 128, LWA_ALPHA);
    }

    // Fill out member variables
    out_window->x = x;
    out_window->y = y;
    out_window->width = w;
    out_window->height = h;
    out_window->flags = info->flags;

    // Set boolean to false, meaning it's currently open
    out_window->should_close = false;

    // Show the window
    ShowWindow(out_window->hwnd, SW_SHOW);

    return true;
}

bool window_create_overlay(platform_state_t *state, window_t *out_window) {
    assert(state);
    assert(out_window && "Out window cannot be NULL");

    DWORD w_style = WS_POPUP;
    DWORD w_ex_style = WS_EX_TOOLWINDOW | WS_EX_TOPMOST;

    int32_t x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int32_t y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    uint32_t w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    uint32_t h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    // Create the window itself
    out_window->hwnd = CreateWindowExW(
        w_ex_style,
        OVERLAY_WIN_CLASS_NAME,
        L"Chroma Scopes - Overlay",
        w_style,
        0, 0,
        0, 0,
        NULL,
        NULL,
        state->h_instance,
        out_window);

    if (!out_window->hwnd) {
        LOG("Window creation failed.");
        return false;
    }

    // Fill out member variables
    out_window->x = x;
    out_window->y = y;
    out_window->width = w;
    out_window->height = h;
    out_window->flags = WINDOW_FLAG_TRANSPARENT | WINDOW_FLAG_NO_TASKBAR_ICON | WINDOW_FLAG_MONITOR_SIZE | WINDOW_FLAG_ALWAYS_ON_TOP | WINDOW_FLAG_BORDERLESS;

    // Set boolean to false, meaning it's currently open
    out_window->should_close = false;

    return true;
}

void window_overlay_show(window_t *window) {
    capture_screen_for_overlay();

    SetWindowPos(window->hwnd, HWND_TOP, window->x, window->y, window->width, window->height, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    ShowWindow(window->hwnd, SW_SHOW);
    SetForegroundWindow(window->hwnd);
    UpdateWindow(window->hwnd);
}

void window_destroy(window_t *window) {
    assert(window && "Window pointer MUST NOT be NULL");
    if (window->hwnd) {
        DestroyWindow(window->hwnd);
    }
    window->hwnd = NULL;
}

void window_proc_messages(window_t *window) {
    MSG msg = {0};
    while (PeekMessageW(&msg, window->hwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

bool window_should_close(window_t *window) {
    assert(window && "Window cannot be NULL");
    return window->should_close;
}

void window_post_close(window_t *window) {
    assert(window);
    PostMessage(window->hwnd, WM_CLOSE, 0, 0);
}

void window_minimize(window_t *window) {
    assert(window);
    ShowWindow(window->hwnd, SW_MINIMIZE);
}

void window_maximize_restore(window_t *window) {
    assert(window);
    WINDOWPLACEMENT placement;
    placement.length = sizeof(placement);
    GetWindowPlacement(window->hwnd, &placement);
    ShowWindow(window->hwnd, placement.showCmd == SW_MAXIMIZE ? SW_RESTORE : SW_MAXIMIZE);
}

void window_set_always_on_top(window_t *window, bool enable) {
    assert(window);
    SetWindowPos(window->hwnd, enable ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

void window_set_custom_dragarea(window_t *window, rect_t area) {
    assert(window);
    window->custom_dragbar = area;
}

window_t *window_get_from_point(int2_t point) {
    POINT cursor_pos = {point.x, point.y};
    HWND target_win = WindowFromPoint(cursor_pos);
    if (target_win) return (window_t *)GetWindowLongPtr(target_win, GWLP_USERDATA);

    return NULL;
}

bool window_get_rect(window_t *window, rect_t *rect) {
    RECT win_rect = {0};
    if (GetWindowRect(window->hwnd, &win_rect)) {
        rect->x = win_rect.left;
        rect->y = win_rect.top;
        rect->width = win_rect.right;
        rect->height = win_rect.bottom;
        return true;
    }
    return false;
}

bool window_set_window_pos(window_t *window, int32_t x, int32_t y) {
    return (bool)SetWindowPos(window->hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

bool window_is_maximized(window_t *window) {
    return (bool)IsZoomed(window->hwnd);
}

int2_t window_client_to_screen(window_t *window, int2_t client_point) {
    POINT win_point = {client_point.x, client_point.y};
    ClientToScreen(window->hwnd, &win_point);
    return (int2_t){win_point.x, win_point.y};
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

double platform_get_seconds(void) {
    LARGE_INTEGER now_time;
    QueryPerformanceCounter(&now_time);
    return (double)now_time.QuadPart * seconds_per_tick;
}

int2_t platform_get_screen_cursor_pos(void) {
    POINT p;
    GetCursorPos(&p);
    return (int2_t){p.x, p.y};
}

static keycode_t vk_to_keycode(WPARAM w_param) {
    switch (w_param) {
        case VK_CONTROL:
            return KEY_CTRL;
        case '0':
            return KEY_0;
        case '1':
            return KEY_1;
        case '2':
            return KEY_2;
        case '3':
            return KEY_3;
        case '4':
            return KEY_4;
        case '5':
            return KEY_5;
        case '6':
            return KEY_6;
        case '7':
            return KEY_7;
        case '8':
            return KEY_8;
        case '9':
            return KEY_9;
        case 'Q':
            return KEY_Q;
        case 'W':
            return KEY_W;
        case 'E':
            return KEY_E;
        case 'R':
            return KEY_R;
        case 'P':
            return KEY_P;
        case 'N':
            return KEY_N;
        default:
            return KEY_UNKNOWN;
    }
}

// TODO: make sure the resizing logic is correct!
static BOOL window_resizing(window_t *window, WPARAM w_param, LPARAM l_param) {
    RECT *rect = (RECT *)l_param;

    int width = rect->right - rect->left;
    int height = rect->bottom - rect->top;
    float aspect_ratio = window->width / (float)window->height;

    // Calculate new dimensions based on which edge is being dragged
    switch (w_param) {
        case WMSZ_LEFT:
        case WMSZ_RIGHT:
            // Width is changing, adjust height
            height = (int)(width / aspect_ratio);
            rect->bottom = rect->top + height;
            break;

        case WMSZ_TOP:
        case WMSZ_BOTTOM:
            // Height is changing, adjust width
            width = (int)(height * aspect_ratio);
            rect->right = rect->left + width;
            break;

        case WMSZ_TOPLEFT:
        case WMSZ_TOPRIGHT:
        case WMSZ_BOTTOMLEFT:
        case WMSZ_BOTTOMRIGHT: {
            // Dragging the corner -- pick the dimension that changed more
            float width_ratio = width / (height * aspect_ratio);
            if (width_ratio > 1.0f) {
                // Width changed more, adjust height
                height = width / aspect_ratio;
                if (w_param == WMSZ_TOPLEFT || w_param == WMSZ_TOPRIGHT) {
                    rect->top = rect->bottom - height;
                } else {
                    rect->bottom = rect->top + height;
                }
            } else {
                // Height changed more, adjust width
                width = height * aspect_ratio;
                if (w_param == WMSZ_TOPLEFT || w_param == WMSZ_BOTTOMLEFT) {
                    rect->left = rect->right - width;
                } else {
                    rect->right = rect->left + width;
                }
            }
        } break;
    }

    return TRUE;
}

static LRESULT CALLBACK winproc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
    // For now I'm passing in window struct so technically that is for creating multiple windows
    // and identifying them but I'm leaving it like this. Later, this could be streamlined if
    // I will only have a single window.
    window_t *window = (window_t *)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW *create_struct = (CREATESTRUCTW *)l_param;
            window_t *win = (window_t *)create_struct->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)win);
            return 0;
        }

        case WM_MOUSEMOVE: {
            int32_t x = GET_X_LPARAM(l_param);
            int32_t y = GET_Y_LPARAM(l_param);
            input_process_mouse_move(x, y);
            return 0;
        }

        case WM_NCHITTEST: {
            POINT screen_point = {GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
            ScreenToClient(hwnd, &screen_point);
            if (rect_contains(window->custom_dragbar, (float2_t){screen_point.x, screen_point.y})) {
                return HTCAPTION;
            }
            return HTCLIENT;
        }

        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP: {
            bool pressed = msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN;
            if (pressed)
                SetCapture(hwnd);
            else
                ReleaseCapture();
            mousebutton_t mb = MOUSE_BUTTON_COUNT;

            switch (msg) {
                case WM_LBUTTONDOWN:
                case WM_LBUTTONUP:
                    mb = MOUSE_BUTTON_LEFT;
                    break;
                case WM_RBUTTONDOWN:
                case WM_RBUTTONUP:
                    mb = MOUSE_BUTTON_RIGHT;
                    break;
                case WM_MBUTTONDOWN:
                case WM_MBUTTONUP:
                    mb = MOUSE_BUTTON_MIDDLE;
                    break;
            }

            if (mb < MOUSE_BUTTON_COUNT) {
                input_process_mouse_button(mb, pressed);
            }

            return 0;
        };

        case WM_KEYDOWN: {
            input_process_key(vk_to_keycode(w_param), true);
            return 0;
        }

        case WM_KEYUP: {
            input_process_key(vk_to_keycode(w_param), false);
            return 0;
        }

        case WM_GETMINMAXINFO: {
            MINMAXINFO *mmi = (MINMAXINFO *)l_param;
            mmi->ptMinTrackSize.x = 500;
            mmi->ptMinTrackSize.y = 500;
            return 0;
        }

        case WM_SIZING: {
            window_resizing(window, w_param, l_param);
            return 0;
        }

        case WM_CLOSE:
            window->should_close = true;
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcW(hwnd, msg, w_param, l_param);
}

bool g_is_dragging = false;
POINT g_start_point = {0, 0};
POINT g_end_point = {0, 0};
RECT g_current_rect = {0, 0, 0, 0};

static LRESULT CALLBACK winproc_overlay(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT client_rect;
            GetClientRect(hwnd, &client_rect);

            HDC mem_dc = CreateCompatibleDC(hdc);
            HBITMAP screenshot_cpy = CopyImage(overlay_screenshot, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
            SelectObject(mem_dc, screenshot_cpy);

            // Draw screenshot
            BitBlt(hdc, 0, 0, client_rect.right, client_rect.bottom, mem_dc, 0, 0, SRCCOPY);

            HDC alpha_dc = CreateCompatibleDC(hdc);
            UCHAR bits[] = {0, 0, 0, 128};
            HBITMAP alpha_bmp = CreateBitmap(1, 1, 1, 32, bits);
            SelectObject(alpha_dc, alpha_bmp);

            BLENDFUNCTION blend_fn = {AC_SRC_OVER, 0, 128, AC_SRC_ALPHA};

            if (g_is_dragging && (g_start_point.x != g_end_point.x && g_start_point.y != g_end_point.y)) {
                int left = MIN(g_start_point.x, g_end_point.x);
                int top = MIN(g_start_point.y, g_end_point.y);
                int right = MAX(g_start_point.x, g_end_point.x);
                int bottom = MAX(g_start_point.y, g_end_point.y);

                if (top > 0) {
                    GdiAlphaBlend(hdc, 0, 0, client_rect.right, top, alpha_dc, 0, 0, 1, 1, blend_fn);
                }

                if (bottom < client_rect.bottom) {
                    GdiAlphaBlend(hdc, 0, bottom, client_rect.right, client_rect.bottom, alpha_dc, 0, 0, 1, 1, blend_fn);
                }

                if (left > 0) {
                    GdiAlphaBlend(hdc, 0, top, left, bottom - top, alpha_dc, 0, 0, 1, 1, blend_fn);
                }

                if (right < client_rect.right) {
                    GdiAlphaBlend(hdc, right, top, client_rect.right - right, bottom - top, alpha_dc, 0, 0, 1, 1, blend_fn);
                }

                HPEN rect_pen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
                HPEN old_pen = (HPEN)SelectObject(hdc, rect_pen);
                HBRUSH old_brush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

                Rectangle(hdc, left, top, right, bottom);

                SelectObject(hdc, old_pen);
                SelectObject(hdc, old_brush);
                DeleteObject(rect_pen);
            } else {
                GdiAlphaBlend(hdc, 0, 0, client_rect.right, client_rect.bottom, alpha_dc, 0, 0, 1, 1, blend_fn);
            }

            // Cleanup
            DeleteObject(alpha_bmp);
            DeleteDC(alpha_dc);
            DeleteObject(screenshot_cpy);
            DeleteDC(mem_dc);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            SetCapture(hwnd);

            if (!g_is_dragging) {
                g_is_dragging = true;
                g_start_point.x = GET_X_LPARAM(l_param);
                g_start_point.y = GET_Y_LPARAM(l_param);

                LOG("Started dragging on Overlay Window at (%d, %d)", g_start_point.x, g_start_point.y);
            }

            return 0;
        }

        case WM_MOUSEMOVE: {
            if (g_is_dragging) {
                g_end_point.x = GET_X_LPARAM(l_param);
                g_end_point.y = GET_Y_LPARAM(l_param);

                InvalidateRect(hwnd, NULL, FALSE);
                UpdateWindow(hwnd);
            }

            return 0;
        }

        case WM_LBUTTONUP: {
            g_is_dragging = false;
            ReleaseCapture();

            g_start_point = (POINT){0, 0};
            g_end_point = (POINT){0, 0};
            // InvalidateRect(hwnd, NULL, TRUE);
            // UpdateWindow(hwnd);

            ShowWindow(hwnd, SW_HIDE);

            return 0;
        }

        case WM_KEYDOWN: {
            if (w_param == VK_ESCAPE) {
                ShowWindow(hwnd, SW_HIDE);
            }
            return 0;
        }

        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }

        default:
            return DefWindowProcW(hwnd, msg, w_param, l_param);
    }
}

static void capture_screen_for_overlay(void) {
    if (overlay_screenshot) {
        DeleteObject(overlay_screenshot);
        overlay_screenshot = NULL;
    }

    int32_t x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int32_t y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    uint32_t w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    uint32_t h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    HDC screen_dc = GetDC(NULL);
    HDC mem_dc = CreateCompatibleDC(screen_dc);
    HBITMAP bmp = CreateCompatibleBitmap(screen_dc, w, h);
    SelectObject(mem_dc, bmp);

    BitBlt(mem_dc, 0, 0, w, h, screen_dc, x, y, SRCCOPY);

    ReleaseDC(NULL, screen_dc);
    DeleteDC(mem_dc);

    overlay_screenshot = bmp;
}
