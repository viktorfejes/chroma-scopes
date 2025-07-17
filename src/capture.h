#pragma once

#include "math.h"

#include <stdbool.h>
#include <stdint.h>

#include <d3d11_1.h>
#include <dxgi1_2.h>

#define CS_MAX_MONITORS 3

struct texture;

typedef struct monitor_info {
    uint32_t id;

    // Display info (from win32)
    char device_name[32];
    rect_t bounds;
    rect_t work_area;
    float dpi;
    bool is_primary;

    // Capture info (DXGI)
    uint32_t adapter_index;
    uint32_t output_index;
    DXGI_OUTPUT_DESC dxgi_desc;
    DXGI_FORMAT native_format;
    bool can_capture;
} monitor_info_t;

typedef struct capture {
    IDXGIOutput1 *output;
    IDXGIOutputDuplication *duplication;
    DXGI_FORMAT format;
    DXGI_OUTDUPL_FRAME_INFO frame_info;

    monitor_info_t monitors[CS_MAX_MONITORS];
    uint32_t monitor_count;
    uint32_t active_monitor;
} capture_t;

bool capture_initialize(ID3D11Device1 *device, capture_t *capture);
void capture_terminate(capture_t *capture);
bool capture_frame(capture_t *capture, rect_t area, ID3D11DeviceContext1 *context, struct texture *out_texture);
bool capture_set_monitor(capture_t *capture, ID3D11Device1 *device, uint8_t monitor_id);
uint32_t capture_enumerate_monitors(monitor_info_t *monitors, uint32_t max_count);
