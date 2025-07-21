#include "window.h"

#include "logger.h"
#include "input.h"

#include <assert.h>
#include <windows.h>

#define DEFAULT_WIN_CLASS_NAME L"DefaultWinClassName"
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

static LRESULT CALLBACK winproc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param);
static BOOL window_resizing(WPARAM w_param, LPARAM l_param);

bool window_create(const char *title, uint16_t width, uint16_t height, window_t *out_window) {
    assert(out_window && "Out window cannot be NULL");

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

    // Adjust window size to account for decoration
    RECT wr = {.left = 0, .top = 0, .right = (LONG)width, .bottom = (LONG)height};
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    // Convert title to wide char from char
    wchar_t title_wide[256];
    MultiByteToWideChar(CP_UTF8, 0, title, -1, title_wide, sizeof(title_wide) / sizeof(wchar_t));

    // Create the window itself
    out_window->hwnd = CreateWindowExW(
        0,
        DEFAULT_WIN_CLASS_NAME,
        title_wide,
        WS_OVERLAPPEDWINDOW,
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
    out_window->width = wr.right - wr.left;
    out_window->height = wr.bottom - wr.top;
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

static BOOL window_resizing(WPARAM w_param, LPARAM l_param) {
    RECT *rect = (RECT *)l_param;

    int width = rect->right - rect->left;
    int height = rect->bottom - rect->top;

    // TEMP: This needs to be changed to use the set window aspect ratio
    float aspect_ratio = 1280.0f / 720.0f;

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

        case WM_GETMINMAXINFO: {
            MINMAXINFO *mmi = (MINMAXINFO *)l_param;
            mmi->ptMinTrackSize.x = 500;
            mmi->ptMinTrackSize.y = 500;
            return 0;
        }

        case WM_SIZING: {
            window_resizing(w_param, l_param);
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
