#include "vri.h"

#include <stdlib.h>
#include <winerror.h>
// TEMP:
#define VRI_ENABLE_D3D11_SUPPORT 1

#if VRI_ENABLE_D3D11_SUPPORT
#include <d3d11.h>
#include <dxgi1_6.h>
#endif

#define ADAPTER_MAX_COUNT 32

#define TYPE_SHIFT 60
#define VRAM_SHIFT 4
#define VENDOR_MASK 0xFull
#define VRAM_MASK 0x0FFFFFFFFFFFFFF0ull

bool none_device_create(const vri_device_desc_t *desc, vri_device_t **device);
bool d3d11_device_create(const vri_device_desc_t *desc, vri_device_t **device);
bool vk_device_create(const vri_device_desc_t *desc, vri_device_t **device);

bool vk_enum_adapters(vri_adapter_desc_t *adapter_desc, uint32_t *adapter_desc_count);

static void *default_allocator_allocate(size_t size, size_t alignment);
static void default_allocator_free(void *memory, size_t size, size_t alignment);
static void default_message_callback(vri_message_severity_t severity, const char *message);
static void setup_callbacks(vri_device_desc_t *desc);
static void finish_device_creation(vri_device_desc_t *desc, vri_device_t **device);

static vri_vendor_t get_vendor_from_id(uint32_t vendor_id) {
    switch (vendor_id) {
        case 0x10DE:
            return VRI_VENDOR_NVIDIA;
        case 0x1002:
            return VRI_VENDOR_AMD;
        case 0x8086:
            return VRI_VENDOR_INTEL;
        default:
            return VRI_VENDOR_UNKNOWN;
    }
}

static int sort_adapters(const void *a, const void *b) {
    const vri_adapter_desc_t *adapter_a = (const vri_adapter_desc_t *)a;
    const vri_adapter_desc_t *adapter_b = (const vri_adapter_desc_t *)b;

    // Priority order: [type:4][vram:56][vendor:4]
    uint64_t type_a = (adapter_a->type == VRI_GPU_TYPE_DISCRETE) ? 1 : 0;
    uint64_t score_a = ((uint64_t)adapter_a->vendor & VENDOR_MASK) |
                       ((adapter_a->vram << VRAM_SHIFT) & VRAM_MASK) |
                       (type_a << TYPE_SHIFT);

    uint64_t type_b = (adapter_b->type == VRI_GPU_TYPE_DISCRETE) ? 1 : 0;
    uint64_t score_b = ((uint64_t)adapter_b->vendor & VENDOR_MASK) |
                       ((adapter_b->vram << VRAM_SHIFT) & VRAM_MASK) |
                       (type_b << TYPE_SHIFT);

    if (score_a > score_b) return -1;
    if (score_a < score_b) return 1;
    return 0;
}

#if VRI_ENABLE_D3D11_SUPPORT
static bool d3d_enum_adapters(vri_adapter_desc_t *adapter_descs, uint32_t *adapter_desc_count) {
    IDXGIFactory4 *dxgi_factory = NULL;
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS_C(IDXGIFactory4, &dxgi_factory));
    if (FAILED(hr)) {
        return false;
    }

    uint32_t adapter_count = 0;
    IDXGIAdapter1 *adapters[ADAPTER_MAX_COUNT];

    for (uint32_t i = 0; i < ADAPTER_MAX_COUNT; ++i) {
        IDXGIAdapter1 *adapter = NULL;
        hr = dxgi_factory->lpVtbl->EnumAdapters1(dxgi_factory, i, &adapter);
        if (hr == DXGI_ERROR_NOT_FOUND) {
            break;
        }

        DXGI_ADAPTER_DESC1 desc = {0};
        if (adapter->lpVtbl->GetDesc1(adapter, &desc) == S_OK) {
            if (desc.Flags == DXGI_ADAPTER_FLAG_NONE) {
                adapters[adapter_count++] = adapter;
            } else {
                adapter->lpVtbl->Release(adapter);
            }
        } else {
            adapter->lpVtbl->Release(adapter);
        }
    }

    if (!adapter_count) {
        dxgi_factory->lpVtbl->Release(dxgi_factory);
        return false;
    }

    vri_adapter_desc_t queried_adapter_descs[ADAPTER_MAX_COUNT];
    uint32_t validated_adapter_count = 0;

    for (uint32_t i = 0; i < adapter_count; ++i) {
        DXGI_ADAPTER_DESC desc = {0};
        adapters[i]->lpVtbl->GetDesc(adapters[i], &desc);

        vri_adapter_desc_t *adapter_desc = &queried_adapter_descs[validated_adapter_count];
        adapter_desc->luid = *(uint64_t *)&desc.AdapterLuid;
        adapter_desc->device_id = desc.DeviceId;
        adapter_desc->vendor = get_vendor_from_id(desc.VendorId);
        adapter_desc->vram = desc.DedicatedVideoMemory;
        adapter_desc->shared_system_memory = desc.SharedSystemMemory;

        // Since queue count cannot be queried in D3D, we'll use some defaults
        adapter_desc->queue_count[VRI_QUEUE_TYPE_GRAPHICS] = 3;
        adapter_desc->queue_count[VRI_QUEUE_TYPE_COMPUTE] = 3;
        adapter_desc->queue_count[VRI_QUEUE_TYPE_TRANSFER] = 3;

        // Get the GPU type by creating a device, at the same time test IF we can create a device
        D3D_FEATURE_LEVEL fl[2] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
        ID3D11Device *device = NULL;

        hr = D3D11CreateDevice((IDXGIAdapter *)adapters[i], D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, fl, 2, D3D11_SDK_VERSION, &device, NULL, NULL);

        if (FAILED(hr)) {
            // Cannot create device on the adapter, so "bin" it
            adapters[i]->lpVtbl->Release(adapters[i]);
            adapters[i] = NULL;
            continue;
        }

        D3D11_FEATURE_DATA_D3D11_OPTIONS2 o2 = {0};
        hr = device->lpVtbl->CheckFeatureSupport(device, D3D11_FEATURE_D3D11_OPTIONS2, &o2, sizeof(o2));
        if (SUCCEEDED(hr)) {
            adapter_desc->type = o2.UnifiedMemoryArchitecture ? VRI_GPU_TYPE_INTEGRATED : VRI_GPU_TYPE_DISCRETE;
        }

        // We don't need the device at this point so just release it
        device->lpVtbl->Release(device);

        validated_adapter_count++;

        // Release the adapter as we are done with it (so we don't have to do this in another loop)
        adapters[i]->lpVtbl->Release(adapters[i]);
    }

    // Clean up factory
    dxgi_factory->lpVtbl->Release(dxgi_factory);

    if (validated_adapter_count == 0) {
        return false;
    }

    // Sort adapters based on some scoring
    qsort(queried_adapter_descs, validated_adapter_count, sizeof(queried_adapter_descs[0]), sort_adapters);

    *adapter_desc_count = MIN(*adapter_desc_count, validated_adapter_count);
    for (uint32_t i = 0; i < *adapter_desc_count; ++i) {
        adapter_descs[i] = queried_adapter_descs[i];
    }

    return true;
}
#endif

bool vri_enumerate_adapters(vri_adapter_desc_t *adapter_descs, uint32_t *adapter_desc_count) {
    bool result = false;

#if VRI_ENABLE_VK_SUPPORT
    // Vulkan return actual capabilities, so let's try this first
    result = vk_enum_adapters(adapter_desc, adapter_desc_count);
#endif

#if VRI_ENABLE_D3D11_SUPPORT
    // Only if Vulkan is not available do we use anything other than...
    if (!result) {
        result = d3d_enum_adapters(adapter_descs, adapter_desc_count);
    }
#endif

#if VRI_ENABLE_NONE_SUPPORT && !(VRI_ENABLE_VK_SUPPORT || VRI_ENABLE_D3D11_SUPPORT)
    if (!result) {
        if (*adapter_desc_count > 1) {
            *adapter_desc_count = 1;
        }

        result = true;
    }
#endif

    return result;
}

bool vri_device_create(const vri_device_desc_t *desc, vri_device_t **device) {
    bool result = false;

    vri_device_desc_t mod_desc = *desc;
    setup_callbacks(&mod_desc);

#if VRI_ENABLE_NONE_SUPPORT
    if (desc->api == VRI_API_NONE)
        result = none_device_create(&mod_desc, device);
#endif

#if VRI_ENABLE_D3D11_SUPPORT
    if (desc->api == VRI_API_D3D11)
        result = d3d11_device_create(&mod_desc, device);
#endif

#if VRI_ENABLE_VK_SUPPORT
    if (desc->api == VRI_API_VULKAN)
        result = vk_device_create(&mod_desc, device);
#endif

    if (!result) return false;

    finish_device_creation(&mod_desc, device);

    return true;
}

void vri_device_destroy(vri_device_t *device) {
    device->core_interface.device_destroy(device);
}

static void setup_callbacks(vri_device_desc_t *desc) {
    if (!desc->allocation_callback.allocate || !desc->allocation_callback.free) {
        desc->allocation_callback.allocate = default_allocator_allocate;
        desc->allocation_callback.free = default_allocator_free;
    }

    if (!desc->debug_callback.message_callback) {
        desc->debug_callback.message_callback = default_message_callback;
    }
}

static void *default_allocator_allocate(size_t size, size_t alignment) {
    (void)alignment;
    return malloc(size);
}

static void default_allocator_free(void *memory, size_t size, size_t alignment) {
    (void)size;
    (void)alignment;
    free(memory);
}

static void default_message_callback(vri_message_severity_t severity, const char *message) {
    (void)severity;
    (void)message;
    // NO-OP
}

static void finish_device_creation(vri_device_desc_t *desc, vri_device_t **device) {
    (*device)->allocation_callback = desc->allocation_callback;
    (*device)->debug_callback = desc->debug_callback;
    (*device)->adapter_desc = desc->adapter_desc;
}
