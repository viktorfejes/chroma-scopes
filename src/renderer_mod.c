#include "renderer_mod.h"

#include "platform.h"
#include "logger.h"

bool renderer_initialize(size_t *memory_requirement, renderer_state_t *state, vri_api_t api) {
    (void)backend_plugin;

    *memory_requirement = sizeof(renderer_state_t);
    if (!state) return true;

    vri_device_desc_t device_desc = {
        .api = api,
    };
    if (!vri_device_create(&device_desc, state->device)) {
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
