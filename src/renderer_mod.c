#include "renderer_mod.h"

#include "platform.h"

// bool renderer_initialize(size_t *memory_requirement, renderer_state_t *state, platform_dynlib_t *backend_plugin) {
//     (void)backend_plugin;
//
//     *memory_requirement = sizeof(renderer_state_t);
//     if (!state) return true;
//
//     // if (!rhi_init_table(backend_plugin, &state->rhi)) {
//     //     LOG("Couldn't query the RHI interface for the desired backend");
//     //     return false;
//     // }
//
//     // Create device
//     if (!state->rhi.create_device(state->device, NULL)) {
//         return false;
//     }
//
//     // Create context/command list
//     // if (!state->rhi.create_command_list(state->command_list, state->device)) {
//     //     return false;
//     // }
//
//     return true;
// }

// void renderer_shutdown(renderer_state_t *state) {
//     if (state) {
//         // state->rhi.destroy_command_list(state->command_list);
//         state->rhi.destroy_device(state->device);
//     }
// }
