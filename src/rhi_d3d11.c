#include "rhi.h"

#include <assert.h>
#include <d3d11_4.h>

struct rhi_device {
    ID3D11Device5 *device;
    ID3D11DeviceContext4 *immediate_context;
    D3D_FEATURE_LEVEL feature_level;

    rhi_message_callback_fn message_callback;
};

struct rhi_queue {
    rhi_device_t *parent_device;
};

#define IID_PPV_ARGS_C(type, ppType) \
    &IID_##type, (void **)(ppType)

bool create_device(rhi_device_t *out_device, rhi_message_callback_fn message_callback) {
    out_device->message_callback = message_callback;

    UINT create_device_flags = 0;
#ifdef _DEBUG
    create_device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    // Hardware first, software as fallback
    D3D_DRIVER_TYPE driver_types[] = {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP};

    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0};

    ID3D11Device *base_device = NULL;
    ID3D11DeviceContext *base_context = NULL;
    D3D_FEATURE_LEVEL achieved_level = {0};

    HRESULT hr = E_FAIL;
    for (int i = 0; i < (int)ARRAYSIZE(driver_types); i++) {
        hr = D3D11CreateDevice(
            NULL,
            driver_types[i],
            NULL,
            create_device_flags,
            feature_levels,
            ARRAYSIZE(feature_levels),
            D3D11_SDK_VERSION,
            &base_device,
            &achieved_level,
            &base_context);

        if (SUCCEEDED(hr)) {
            out_device->message_callback(RHI_MESSAGE_SEVERITY_INFO, "D3D11 base device created successfully.");
            break;
        }
    }

    if (FAILED(hr)) {
        out_device->message_callback(RHI_MESSAGE_SEVERITY_FATAL, "Failed to create D3D11 device with any driver type");
        return false;
    }

    // Save achieved feature level in case it's needed
    out_device->feature_level = achieved_level;

    // Upgrade to D3D11.4 - Device5
    ID3D11Device5 *device = NULL;
    hr = base_device->lpVtbl->QueryInterface(base_device, IID_PPV_ARGS_C(ID3D11Device5, &device));
    if (FAILED(hr)) {
        out_device->message_callback(RHI_MESSAGE_SEVERITY_FATAL, "Couldn't upgrade to ID3D11Device5. Feature not supported");
        base_device->lpVtbl->Release(base_device);
        base_context->lpVtbl->Release(base_context);
        return false;
    }

    // Upgrade Immediate Device Context, as well.
    ID3D11DeviceContext4 *context = NULL;
    hr = base_context->lpVtbl->QueryInterface(base_context, IID_PPV_ARGS_C(ID3D11DeviceContext4, &context));
    if (FAILED(hr)) {
        out_device->message_callback(RHI_MESSAGE_SEVERITY_FATAL, "Couldn't upgrade to ID3D11DeviceContext4. Feature not supported");
        out_device->device->lpVtbl->Release(out_device->device);
        base_device->lpVtbl->Release(base_device);
        base_context->lpVtbl->Release(base_context);
        return false;
    }

    out_device->device = device;
    out_device->immediate_context = context;

#ifdef _DEBUG
    // Set up enhanced debug layer
    ID3D11InfoQueue *info_queue = NULL;
    hr = out_device->device->lpVtbl->QueryInterface(out_device->device, IID_PPV_ARGS_C(ID3D11InfoQueue, &info_queue));

    if (SUCCEEDED(hr) && info_queue) {
        // No breaking on errors, only logging
        info_queue->lpVtbl->SetBreakOnSeverity(info_queue, D3D11_MESSAGE_SEVERITY_CORRUPTION, FALSE);
        info_queue->lpVtbl->SetBreakOnSeverity(info_queue, D3D11_MESSAGE_SEVERITY_ERROR, FALSE);
        info_queue->lpVtbl->SetBreakOnSeverity(info_queue, D3D11_MESSAGE_SEVERITY_WARNING, FALSE);

        // Enable message storage so we can retrieve them
        info_queue->lpVtbl->SetMuteDebugOutput(info_queue, FALSE);

        // Set storage limit
        info_queue->lpVtbl->SetMessageCountLimit(info_queue, 1024);

        out_device->message_callback(RHI_MESSAGE_SEVERITY_INFO, "D3D11 debug layer enabled for logging");
        info_queue->lpVtbl->Release(info_queue);
    } else {
        out_device->message_callback(RHI_MESSAGE_SEVERITY_ERROR, "Failed to enable D3D11 debug layer");
    }
#endif

    // Release remaining not needed resources
    base_device->lpVtbl->Release(base_device);
    base_context->lpVtbl->Release(base_context);

    return true;
}

void destroy_device(rhi_device_t *device) {
    if (device->device) device->device->lpVtbl->Release(device->device);
}

bool create_queue(rhi_queue_t *out_queue, rhi_device_t *device) {
    if (device) {
        out_queue->parent_device = device;
        return true;
    }
    return false;
}

void destroy_queue(rhi_queue_t *queue) {
    if (queue) {
        queue->parent_device->immediate_context->lpVtbl->Release(queue->parent_device->immediate_context);
    }
}
