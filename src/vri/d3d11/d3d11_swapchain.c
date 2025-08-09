#include "../vri.h"

#include "d3d11_device.h"

#include <assert.h>

#include <d3d11_4.h>
#include <dxgi1_5.h>

typedef struct {
    vri_swapchain_t base;

    IDXGISwapChain4 *swapchain;
    IDXGIFactory2 *factory2;
    uint32_t flags;
    void *hwnd;
} vri_d3d11_swapchain_t;

const DXGI_FORMAT swapchain_format_lut[] = {
    DXGI_FORMAT_R8G8B8A8_UNORM,
    DXGI_FORMAT_R16G16B16A16_FLOAT,
};

bool d3d11_swapchain_create(vri_device_t *device, const struct vri_swapchain_desc *swapchain_desc, struct vri_swapchain **out_swapchain) {
    // TODO: Definiteily need encapsulation/convenience function for logging
    // Or a macro
    vri_debug_callback_t dbg = device->debug_callback;

    HWND hwnd = swapchain_desc->window.win32.hwnd;
    assert(hwnd);

    // TODO: Encapsulate into a getter on the device?
    IDXGIAdapter *adapter = ((vri_d3d11_device_t *)device)->adapter;

    // Get the factory from the adapter
    IDXGIFactory2 *factory2 = NULL;
    HRESULT hr = adapter->lpVtbl->GetParent(adapter, IID_PPV_ARGS_C(IDXGIFactory2, &factory2));
    if (FAILED(hr)) {
        dbg.message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Failed to get DXGI Factory2");
        return false;
    }

    DXGI_FORMAT format = swapchain_format_lut[(uint32_t)swapchain_desc->format];

    DXGI_SWAP_CHAIN_DESC1 desc = {
        .Width = swapchain_desc->width,   // Zero here means it defaults to window width
        .Height = swapchain_desc->height, // ... and to window height.
        .Format = format,
        // I'm so confused by this BufferCount and whether it includes front buffer or not...
        .BufferCount = swapchain_desc->texture_count,
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .SampleDesc = {
            .Count = 1,
            .Quality = 0},
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .Scaling = DXGI_SCALING_NONE,
        .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
    };

    // Create the base swapchain
    IDXGISwapChain1 *swapchain1 = NULL;
    hr = factory2->lpVtbl->CreateSwapChainForHwnd(factory2, (IUnknown *)device, hwnd, &desc, NULL, NULL, &swapchain1);
    if (FAILED(hr)) {
        dbg.message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Failed to create base swapchain");
        factory2->lpVtbl->Release(factory2);
        return false;
    }

    // Upgrade to Swapchain4
    IDXGISwapChain4 *swapchain4 = NULL;
    hr = swapchain1->lpVtbl->QueryInterface(swapchain1, IID_PPV_ARGS_C(IDXGISwapChain4, &swapchain4));
    if (FAILED(hr)) {
        dbg.message_callback(VRI_MESSAGE_SEVERITY_ERROR, "Failed to update base swapchain to Swapchain4");
        swapchain1->lpVtbl->Release(swapchain1);
        factory2->lpVtbl->Release(factory2);
        return false;
    }

    // Allocate the internal structure for the swapchain now that we are ok with the rest
    vri_d3d11_swapchain_t *internal = device->allocation_callback.allocate(sizeof(vri_swapchain_t), 8);
    if (!internal) {
        dbg.message_callback(VRI_MESSAGE_SEVERITY_FATAL, "Allocation for internal swapchain struct failed.");
        factory2->lpVtbl->Release(factory2);
        return false;
    }

    // Save out the created pointer
    internal->hwnd = hwnd;
    internal->swapchain = swapchain4;
    internal->factory2 = factory2;
    internal->base.parent_device = device;
    *out_swapchain = (vri_swapchain_t *)internal;

    // Release temporaries
    swapchain1->lpVtbl->Release(swapchain1);

    return true;
}
