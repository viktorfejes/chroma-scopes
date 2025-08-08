#include "vri.h"

#include <stdlib.h>

// TEMP:
#define VRI_ENABLE_D3D11_SUPPORT 1

bool none_device_create(const vri_device_desc_t *desc, vri_device_t **device);
bool d3d11_device_create(const vri_device_desc_t *desc, vri_device_t **device);
bool vk_device_create(const vri_device_desc_t *desc, vri_device_t **device);

static void *default_allocator_allocate(size_t size, size_t alignment);
static void default_allocator_free(void *memory, size_t size, size_t alignment);
static void default_message_callback(vri_message_severity_t severity, const char *message);
static void setup_callbacks(vri_device_desc_t *desc);
static void finish_device_creation(vri_device_desc_t *desc, vri_device_t **device);

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
    device->core_interface.destroy_device(device);
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
