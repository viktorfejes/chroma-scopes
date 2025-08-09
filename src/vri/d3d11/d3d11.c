#include "../vri.h"

#include <assert.h>
#include <d3d11_4.h>

typedef struct {
    vri_device_t base;

    ID3D11Device5 *device;
    ID3D11DeviceContext4 *immediate_context;
    IDXGIAdapter *adapter;
    D3D_FEATURE_LEVEL feature_level;
} vri_d3d11_device_t;

typedef struct {
    vri_d3d11_device_t *parent_device;
} vri_d3d11_context_t;

static void fill_vtable_core(vri_core_interface_t *vtable);

bool d3d11_device_create(const vri_device_desc_t *desc, vri_device_t **device) {
    // Convinience assignment for the debug messages
    vri_debug_callback_t dbg = desc->debug_callback;

    // Attempt to allocate the full internal struct
    vri_d3d11_device_t *impl = desc->allocation_callback.allocate(sizeof(*impl), 8);
    if (!impl) {
        dbg.message_callback(RHI_MESSAGE_SEVERITY_FATAL, "Allocation for device struct failed.");
        return false;
    }

    // Identify the adapter in the API specific way
    // TODO: Add fallback when we just want an adapter so luid == 0
    // In that case we can use NULL for the adapter and D3D_DRIVER_TYPE_HARDWARE
    {
        vri_adapter_desc_t adapter_desc = desc->adapter_desc;
        IDXGIFactory4 *dxgi_factory = NULL;
        HRESULT hr = CreateDXGIFactory2(desc->enable_api_validation ? D3D11_CREATE_DEVICE_DEBUG : 0, IID_PPV_ARGS_C(IDXGIFactory4, &dxgi_factory));
        if (FAILED(hr)) {
            dbg.message_callback(RHI_MESSAGE_SEVERITY_FATAL, "Failed to create DXGIFactory2 for adapter identification");
            return false;
        }

        // LUID luid = *(LUID *)&adapter_desc.luid;
        LUID luid = {
            .LowPart = (DWORD)(adapter_desc.luid & 0xFFFFFFFF),
            .HighPart = (LONG)(adapter_desc.luid >> 32),
        };
        hr = dxgi_factory->lpVtbl->EnumAdapterByLuid(dxgi_factory, luid, IID_PPV_ARGS_C(IDXGIAdapter, &impl->adapter));
        if (FAILED(hr)) {
            dbg.message_callback(RHI_MESSAGE_SEVERITY_FATAL, "Couldn't get IDXGIAdapter");
            return false;
        }

        dxgi_factory->lpVtbl->Release(dxgi_factory);
    }

    UINT create_device_flags = desc->enable_api_validation ? D3D11_CREATE_DEVICE_DEBUG : 0;
    D3D_FEATURE_LEVEL feature_levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};

    ID3D11Device *base_device = NULL;
    ID3D11DeviceContext *base_context = NULL;
    D3D_FEATURE_LEVEL achieved_level = {0};

    HRESULT hr = D3D11CreateDevice(
        impl->adapter,
        D3D_DRIVER_TYPE_UNKNOWN,
        NULL,
        create_device_flags,
        feature_levels,
        ARRAYSIZE(feature_levels),
        D3D11_SDK_VERSION,
        &base_device,
        &achieved_level,
        &base_context);

    if (FAILED(hr)) {
        dbg.message_callback(RHI_MESSAGE_SEVERITY_FATAL, "Failed to create D3D11 device with any driver type");
        desc->allocation_callback.free(impl, sizeof(*impl), 8);
        return false;
    }

    // Upgrade to D3D11.4 - Device5
    ID3D11Device5 *device5 = NULL;
    hr = base_device->lpVtbl->QueryInterface(base_device, IID_PPV_ARGS_C(ID3D11Device5, &device5));
    if (FAILED(hr)) {
        dbg.message_callback(RHI_MESSAGE_SEVERITY_FATAL, "Couldn't upgrade to ID3D11Device5. Feature not supported");
        desc->allocation_callback.free(impl, sizeof(*impl), 8);
        base_device->lpVtbl->Release(base_device);
        base_context->lpVtbl->Release(base_context);
        return false;
    }

    // Upgrade Immediate Device Context, as well.
    ID3D11DeviceContext4 *context4 = NULL;
    hr = base_context->lpVtbl->QueryInterface(base_context, IID_PPV_ARGS_C(ID3D11DeviceContext4, &context4));
    if (FAILED(hr)) {
        dbg.message_callback(RHI_MESSAGE_SEVERITY_FATAL, "Couldn't upgrade to ID3D11DeviceContext4. Feature not supported");
        desc->allocation_callback.free(impl, sizeof(*impl), 8);
        device5->lpVtbl->Release(device5);
        base_device->lpVtbl->Release(base_device);
        base_context->lpVtbl->Release(base_context);
        return false;
    }

    if (desc->enable_api_validation) {
        // Set up enhanced debug layer
        ID3D11InfoQueue *info_queue = NULL;
        hr = impl->device->lpVtbl->QueryInterface(impl->device, IID_PPV_ARGS_C(ID3D11InfoQueue, &info_queue));

        if (SUCCEEDED(hr) && info_queue) {
            // No breaking on errors, only logging
            info_queue->lpVtbl->SetBreakOnSeverity(info_queue, D3D11_MESSAGE_SEVERITY_CORRUPTION, FALSE);
            info_queue->lpVtbl->SetBreakOnSeverity(info_queue, D3D11_MESSAGE_SEVERITY_ERROR, FALSE);
            info_queue->lpVtbl->SetBreakOnSeverity(info_queue, D3D11_MESSAGE_SEVERITY_WARNING, FALSE);

            // Enable message storage so we can retrieve them
            info_queue->lpVtbl->SetMuteDebugOutput(info_queue, FALSE);

            // Set storage limit
            info_queue->lpVtbl->SetMessageCountLimit(info_queue, 1024);

            dbg.message_callback(RHI_MESSAGE_SEVERITY_INFO, "D3D11 debug layer enabled for logging");
            info_queue->lpVtbl->Release(info_queue);
        } else {
            dbg.message_callback(RHI_MESSAGE_SEVERITY_ERROR, "Failed to enable D3D11 debug layer");
        }
    }

    // Fill out the known fields
    impl->base.api = VRI_API_D3D11;
    impl->device = device5;
    impl->immediate_context = context4;
    impl->feature_level = achieved_level;

    // Fill the core interface so device has a way of reaching it
    fill_vtable_core(&impl->base.core_interface);

    // Save out the created pointer
    *device = (vri_device_t *)impl;

    // Release remaining not needed resources
    base_device->lpVtbl->Release(base_device);
    base_context->lpVtbl->Release(base_context);

    return true;
}

void d3d11_device_destroy(vri_device_t *device) {
    if (device) {
        vri_d3d11_device_t *impl = (vri_d3d11_device_t *)device;

        if (impl) {
            // Release the GPU resources
            if (impl->immediate_context) impl->immediate_context->lpVtbl->Release(impl->immediate_context);
            if (impl->device) impl->device->lpVtbl->Release(impl->device);

            // Release the full struct on the CPU using the allocator
            impl->base.allocation_callback.free(impl, sizeof(*impl), 8);
        }
    }
}

static void fill_vtable_core(vri_core_interface_t *vtable) {
    vtable->device_destroy = d3d11_device_destroy;
}
