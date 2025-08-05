#include "capture.h"

#include "logger.h"
#include "macros.h"
#include "math.h"
#include "texture.h"

#include <assert.h>

#include <shellscalingapi.h>

struct monitor_enum_context {
    monitor_info_t *monitors;
    uint32_t count;
    uint32_t max_count;
};

static BOOL CALLBACK monitor_enum_proc(HMONITOR hmon, HDC hdc, LPRECT rect, LPARAM data);

bool capture_initialize(ID3D11Device1 *device, capture_t *capture) {
    // Enumerate monitors
    if ((capture->monitor_count = capture_enumerate_monitors(capture->monitors, CS_MAX_MONITORS)) == 0) {
        LOG("No monitors found");
        return false;
    }

    LOG("%u monitors found", capture->monitor_count);
    for (uint32_t i = 0; i < capture->monitor_count; ++i) {
        LOG("Monitor %u: %s", i, capture->monitors[i].device_name);
        LOG("  Position: (%d, %d) Size: %dx%d",
            capture->monitors[i].bounds.x, capture->monitors[i].bounds.y,
            capture->monitors[i].bounds.width, capture->monitors[i].bounds.height);
        LOG("  Can capture: %s", capture->monitors[i].can_capture ? "Yes" : "No");

        if (capture->monitors[i].can_capture) {
            LOG("  DXGI: Adapter %u, Output %u",
                capture->monitors[i].adapter_index, capture->monitors[i].output_index);
        }
    }

    // Set the capture monitor as id 0 by default
    if (!capture_set_monitor(capture, device, 0)) {
        LOG("Couldn't set the capture monitor");
        return false;
    }

    return true;
}

void capture_terminate(capture_t *capture) {
    if (capture) {
        if (capture->duplication) {
            capture->duplication->lpVtbl->Release(capture->duplication);
        }
        if (capture->output) {
            capture->output->lpVtbl->Release(capture->output);
        }
    }
}

bool capture_frame(capture_t *capture, rect_t area, ID3D11DeviceContext1 *context, struct texture *out_texture) {
    assert(out_texture && "Output texture must be available and cannot be NULL");
    assert(out_texture->texture && "Texture is in an invalid state");

    monitor_info_t *monitor = &capture->monitors[capture->active_monitor];

    // Make sure our area is valid
    // TODO: Should I check x and y position as well?
    if (area.width <= 0 || area.height <= 0 || area.width > monitor->bounds.width || area.height > monitor->bounds.height) {
        LOG("Frame couldn't be captured because the specified area is invalid");
        return false;
    }

    // Check the output texture's dimensions against the capture area (They should match or be bigger!)
    if (out_texture->width < area.width || out_texture->height < area.height) {
        LOG("The output texture dimensions do not match the capture region size");
        return false;
    }

    // Acquire next frame
    IDXGIResource *desktop_resource = NULL;
    HRESULT hr = capture->duplication->lpVtbl->AcquireNextFrame(capture->duplication, 50, &capture->frame_info, &desktop_resource);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        // Not an error, we just have no new frames...
        return true;
    }

    if (FAILED(hr)) {
        LOG("Failed to acquire next frame");
        desktop_resource->lpVtbl->Release(desktop_resource);
        return false;
    }

    // Get texture interface
    ID3D11Texture2D *desktop_texture = NULL;
    hr = desktop_resource->lpVtbl->QueryInterface(desktop_resource, IID_PPV_ARGS_C(ID3D11Texture2D, &desktop_texture));
    if (FAILED(hr)) {
        LOG("Failed to get desktop texture");
        desktop_resource->lpVtbl->Release(desktop_resource);
        return false;
    }

    // Describe the crop region
    // TODO: This could be flawed logically, as the area is being stored
    // based on the full desktop so x and y could start with non-zero...
    D3D11_BOX src_box = {
        .top = area.y,
        .left = area.x,
        .right = area.x + area.width,
        .bottom = area.y + area.height,
        .front = 0,
        .back = 1,
    };

    // Copy the selected region
    context->lpVtbl->CopySubresourceRegion(context,
                                           (ID3D11Resource *)out_texture->texture, 0, 0, 0, 0,
                                           (ID3D11Resource *)desktop_texture, 0, &src_box);

    // Cleanup
    desktop_texture->lpVtbl->Release(desktop_texture);
    desktop_resource->lpVtbl->Release(desktop_resource);
    capture->duplication->lpVtbl->ReleaseFrame(capture->duplication);

    return true;
}

bool capture_set_monitor(capture_t *capture, ID3D11Device1 *device, uint8_t monitor_id) {
    // Validate monitor id
    if (monitor_id >= capture->monitor_count) {
        LOG("Invalid monitor ID %u (max: %u)", monitor_id, capture->monitor_count);
        return false;
    }

    // Check if monitor can be captured
    if (!capture->monitors[monitor_id].can_capture) {
        LOG("Monitor %u cannot be captured", monitor_id);
        return false;
    }

    // See if the "new" monitor we are settings is actually a new monitor, not the previously set one
    if (capture->active_monitor == monitor_id && capture->duplication && capture->output) {
        LOG("Capture monitor has not been changed");
        return true;
    }

    // Clean up existing capture resources
    if (capture->duplication) {
        capture->duplication->lpVtbl->Release(capture->duplication);
        capture->duplication = NULL;
    }
    if (capture->output) {
        capture->output->lpVtbl->Release(capture->output);
        capture->output = NULL;
    }

    // Get the monitor info
    monitor_info_t *monitor = &capture->monitors[monitor_id];

    // Create DXGI factory to get the specific adapter
    IDXGIFactory *factory = NULL;
    HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS_C(IDXGIFactory, &factory));
    if (FAILED(hr)) {
        LOG("Failed to create DXGIFactory");
        return false;
    }

    // Get DXGI Adapter for this monitor
    IDXGIAdapter *adapter = NULL;
    hr = factory->lpVtbl->EnumAdapters(factory, monitor->adapter_index, &adapter);
    if (FAILED(hr)) {
        LOG("Failed to get DXGI Adapter %u for monitor %u", monitor->adapter_index, monitor_id);
        factory->lpVtbl->Release(factory);
        return false;
    }

    // Get the output for the selected monitor
    IDXGIOutput *output = NULL;
    hr = adapter->lpVtbl->EnumOutputs(adapter, monitor->output_index, &output);
    if (FAILED(hr)) {
        LOG("Failed to get output %u on adapter %u for monitor %u", monitor->output_index, monitor->adapter_index, monitor_id);
        adapter->lpVtbl->Release(adapter);
        factory->lpVtbl->Release(factory);
        return false;
    }

    // Upgrade the output to IDXGIOutput1 interface
    IDXGIOutput1 *output1 = NULL;
    hr = output->lpVtbl->QueryInterface(output, IID_PPV_ARGS_C(IDXGIOutput1, &output1));
    if (FAILED(hr)) {
        LOG("Failed to get upgraded IDXGIOutput1 interface");
        output->lpVtbl->Release(output);
        adapter->lpVtbl->Release(adapter);
        factory->lpVtbl->Release(factory);
        return false;
    }

    // Create the new duplication interface and cache it
    hr = output1->lpVtbl->DuplicateOutput(output1, (IUnknown *)device, &capture->duplication);
    if (FAILED(hr)) {
        LOG("Failed to create new duplication interface for monitor %u (HRESULT: 0x%08x)", monitor_id, hr);
        output1->lpVtbl->Release(output1);
        output->lpVtbl->Release(output);
        adapter->lpVtbl->Release(adapter);
        factory->lpVtbl->Release(factory);
        return false;
    }

    // Store the output interface for later use
    capture->output = output1;

    // Get and store the output description for capture setup
    DXGI_OUTDUPL_DESC desc = {0};
    capture->duplication->lpVtbl->GetDesc(capture->duplication, &desc);
    capture->format = desc.ModeDesc.Format;

    // Update active monitor index
    capture->active_monitor = monitor_id;

    LOG("Successfully set capture to monitor %u (adapter %u, output %u)", monitor_id, monitor->adapter_index, monitor->output_index);

    // Cleanup
    output->lpVtbl->Release(output);
    adapter->lpVtbl->Release(adapter);
    factory->lpVtbl->Release(factory);

    return true;
}

uint32_t capture_enumerate_monitors(monitor_info_t *monitors, uint32_t max_count) {
    // First, enumerate displays using Win32 for extra info
    struct monitor_enum_context ctx = {
        .monitors = monitors,
        .count = 0,
        .max_count = max_count,
    };

    EnumDisplayMonitors(NULL, NULL, monitor_enum_proc, (LPARAM)&ctx);
    uint32_t win32_monitor_count = ctx.count;

    // Now, enumerate DXGI outputs and correlate with Win32 monitors
    IDXGIFactory *factory = NULL;
    HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS_C(IDXGIFactory, &factory));
    if (FAILED(hr)) {
        LOG("Failed to create DXGIFactory for output enumeration");
        return 0;
    }

    uint32_t adapter_idx = 0;
    IDXGIAdapter *adapter = NULL;

    // Enumerate adapters
    while (factory->lpVtbl->EnumAdapters(factory, adapter_idx, &adapter) != DXGI_ERROR_NOT_FOUND) {
        uint32_t output_idx = 0;
        IDXGIOutput *output = NULL;

        // Enumerate outputs on this adapter
        while (adapter->lpVtbl->EnumOutputs(adapter, output_idx, &output) != DXGI_ERROR_NOT_FOUND) {
            DXGI_OUTPUT_DESC dxgi_desc = {0};
            hr = output->lpVtbl->GetDesc(output, &dxgi_desc);

            if (SUCCEEDED(hr)) {
                // Find the matching monitor we have
                for (uint32_t i = 0; i < win32_monitor_count; ++i) {
                    wchar_t wide_device_name[32];
                    MultiByteToWideChar(CP_UTF8, 0, monitors[i].device_name, -1, wide_device_name, sizeof(wide_device_name) / sizeof(wchar_t));

                    if (wcscmp(dxgi_desc.DeviceName, wide_device_name) == 0) {
                        // Found matching monitor
                        monitors[i].adapter_index = adapter_idx;
                        monitors[i].output_index = output_idx;
                        monitors[i].dxgi_desc = dxgi_desc;

                        // Register as a capturable monitor
                        monitors[i].can_capture = true;

                        break;
                    }
                }
            }

            output->lpVtbl->Release(output);
            output_idx++;
        }

        adapter->lpVtbl->Release(adapter);
        adapter_idx++;
    }

    factory->lpVtbl->Release(factory);
    return win32_monitor_count;
}

monitor_info_t *capture_find_best_monitor_for_rect(capture_t *capture, rect_t selection) {
    monitor_info_t *m = NULL;
    int32_t best_m_area = 0;

    for (uint32_t i = 0; i < capture->monitor_count; ++i) {
        int32_t a = rect_intersection_area(selection, capture->monitors[i].bounds);
        if (a > best_m_area) {
            best_m_area = a;
            m = &capture->monitors[i];
        }
    }

    return m;
}

static BOOL CALLBACK monitor_enum_proc(HMONITOR hmon, HDC hdc, LPRECT rect, LPARAM data) {
    UNUSED(rect);
    UNUSED(hdc);

    struct monitor_enum_context *ctx = (struct monitor_enum_context *)data;

    if (ctx->count >= ctx->max_count) {
        return FALSE; // Reached max number of monitors
    }

    monitor_info_t *monitor = &ctx->monitors[ctx->count];

    // Get info from Win32
    MONITORINFOEXW mi = {.cbSize = sizeof(MONITORINFOEXW)};
    if (!GetMonitorInfoW(hmon, (MONITORINFO *)&mi)) {
        return TRUE; // Continue to next monitor
    }

    // Store Win32 info
    monitor->bounds.x = mi.rcMonitor.left;
    monitor->bounds.y = mi.rcMonitor.top;
    monitor->bounds.width = mi.rcMonitor.right - mi.rcMonitor.left;
    monitor->bounds.height = mi.rcMonitor.bottom - mi.rcMonitor.top;

    monitor->work_area.x = mi.rcWork.left;
    monitor->work_area.y = mi.rcWork.top;
    monitor->work_area.width = mi.rcWork.right - mi.rcWork.left;
    monitor->work_area.height = mi.rcWork.bottom - mi.rcWork.top;

    monitor->is_primary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;

    // Copy device name
    WideCharToMultiByte(CP_UTF8, 0, mi.szDevice, -1, monitor->device_name, sizeof(monitor->device_name), NULL, NULL);

    // Get DPI
    UINT dpi_x, dpi_y;
    if (GetDpiForMonitor(hmon, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y) == S_OK) {
        monitor->dpi = dpi_x;
    } else {
        monitor->dpi = 96.0f;
    }

    // Mark as not capable of capture (it will be decided later)
    monitor->can_capture = false;
    monitor->id = ctx->count;

    ctx->count++;
    return TRUE;
}
