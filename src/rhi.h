#pragma once

#include <stdbool.h>

typedef enum {
    RHI_MESSAGE_SEVERITY_INFO,
    RHI_MESSAGE_SEVERITY_WARNING,
    RHI_MESSAGE_SEVERITY_ERROR,
    RHI_MESSAGE_SEVERITY_FATAL,
} rhi_message_severity_t;

typedef void (*rhi_message_callback_fn)(rhi_message_severity_t severity, const char *message);

typedef struct rhi_device rhi_device_t;
typedef struct rhi_command_buffer rhi_command_buffer_t; // Deferred (in d3d11)
typedef struct rhi_queue rhi_queue_t;                   // Immediate (in d3d11)

typedef struct rhi_swapchain rhi_swapchain_t;

typedef struct rhi_buffer rhi_buffer_t;
typedef struct rhi_texture rhi_texture_t;

typedef struct rhi_descriptor rhi_descriptor_t;
typedef struct rhi_pipeline rhi_pipeline_t;

typedef struct rhi_command_pool rhi_command_pool_t;
typedef struct rhi_fence rhi_fence_t;

typedef struct rhi_api {
    bool (*create_device)(rhi_device_t *out_device, rhi_message_callback_fn message_callback);
    void (*destroy_device)(rhi_device_t *device);

    bool (*create_command_buffer)(rhi_command_buffer_t *out_command_buffer, rhi_device_t *device);
    void (*destroy_command_buffer)(rhi_command_buffer_t *command_buffer);

    bool (*create_queue)(rhi_queue_t *out_queue, rhi_device_t *device);
    void (*destroy_queue)(rhi_queue_t *queue);

    bool (*create_swapchain)(rhi_swapchain_t *out_swapchain, rhi_device_t *device);
    void (*destroy_swapchain)(rhi_swapchain_t *swapchain);
} rhi_api_t;
