#include "renderer_mod.h"

#include "logger.h"
#include "macros.h"

bool renderer_initialize(size_t *memory_requirement, renderer_state_t *state, vri_api_t api) {
    (void)api;

    *memory_requirement = sizeof(renderer_state_t);
    if (!state) return true;

    vri_adapter_desc_t adapter_descs[2];
    uint32_t adapter_desc_count = ARRAY_LENGTH(adapter_descs);
    if (!vri_enumerate_adapters(adapter_descs, &adapter_desc_count)) {
        LOG("Something went wrong while enumerating GPU physical devices");
        return false;
    }

    vri_device_desc_t device_desc = {
        .api = api,
        .adapter_desc = adapter_descs[0],
    };
    if (!vri_device_create(&device_desc, &state->device)) {
        LOG("Couldn't create D3D11 device");
        return false;
    }

    return true;
}

void renderer_shutdown(renderer_state_t *state) {
    if (state) {
        // Finally, destroy the device
        vri_device_destroy(state->device);
    }
}
