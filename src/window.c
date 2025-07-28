#include "window.h"

#include "input.h"
#include "logger.h"

#include <assert.h>
#include <shellscalingapi.h>
#include <windows.h>

#define DEFAULT_WIN_CLASS_NAME L"DefaultWinClassName"
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

static double seconds_per_tick; // 1.0 / frequency (precomputed)

static keycode_t vk_to_keycode(WPARAM w_param);
static LRESULT CALLBACK winproc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param);
static BOOL window_resizing(window_t *window, WPARAM w_param, LPARAM l_param);

bool window_create(const char *title, uint16_t width, uint16_t height, window_t *out_window) {
    assert(out_window && "Out window cannot be NULL");

    // SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HINSTANCE h_instance = GetModuleHandle(NULL);

    // Register a window class
    WNDCLASSEXW wc = {
        .cbSize = sizeof(WNDCLASSEXW),
        .style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
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

    DWORD w_style = WS_POPUP | WS_VISIBLE;
    DWORD w_ex_style = WS_EX_APPWINDOW;

    // Adjust window size to account for decoration
    RECT wr = {.left = 0, .top = 0, .right = (LONG)width, .bottom = (LONG)height};
    AdjustWindowRect(&wr, w_style, FALSE);

    // Convert title to wide char from char
    wchar_t title_wide[256];
    MultiByteToWideChar(CP_UTF8, 0, title, -1, title_wide, sizeof(title_wide) / sizeof(wchar_t));

    // Create the window itself
    out_window->hwnd = CreateWindowExW(
        w_ex_style,
        DEFAULT_WIN_CLASS_NAME,
        title_wide,
        w_style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left,
        wr.bottom - wr.top,
        NULL,
        NULL,
        h_instance,
        out_window);

    if (!out_window->hwnd) {
        LOG("Window creation failed.");
        return false;
    }

    // Fill out member variables
    out_window->width = width;
    out_window->height = height;
    out_window->x = CW_USEDEFAULT;
    out_window->y = CW_USEDEFAULT;

    // Set boolean to false, meaning it's currently open
    out_window->should_close = false;

    // Show the window
    ShowWindow(out_window->hwnd, SW_SHOW);

    return true;
}

void window_destroy(window_t *window) {
    assert(window && "Window pointer MUST NOT be NULL");
    if (window->hwnd) {
        DestroyWindow(window->hwnd);
    }
    window->hwnd = NULL;
}

void window_proc_messages(void) {
    MSG msg = {0};
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
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

bool platform_initialize(void) {
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    seconds_per_tick = 1.0 / (double)frequency.QuadPart;

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

double platform_get_seconds(void) {
    LARGE_INTEGER now_time;
    QueryPerformanceCounter(&now_time);
    return (double)now_time.QuadPart * seconds_per_tick;
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
