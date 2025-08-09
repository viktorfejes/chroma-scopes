#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define IID_PPV_ARGS_C(type, ppType) \
    &IID_##type, (void **)(ppType)

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

// ==========================================================
// SHARED
// ==========================================================
typedef enum {
    VRI_RESULT_SUCCESS = 0,
} vri_result_t;

typedef enum {
    VRI_API_NONE,
    VRI_API_D3D11,
    VRI_API_VK,
} vri_api_t;

typedef enum {
    VRI_GPU_TYPE_UNKNOWN,
    VRI_GPU_TYPE_INTEGRATED,
    VRI_GPU_TYPE_DISCRETE,
} vri_gpu_type_t;

typedef enum {
    VRI_VENDOR_UNKNOWN,
    VRI_VENDOR_INTEL,
    VRI_VENDOR_AMD,
    VRI_VENDOR_NVIDIA
} vri_vendor_t;

typedef enum {
    VRI_QUEUE_TYPE_GRAPHICS,
    VRI_QUEUE_TYPE_COMPUTE,
    VRI_QUEUE_TYPE_TRANSFER,
    VRI_QUEUE_TYPE_COUNT
} vri_queue_type_t;

typedef enum {
    RHI_MESSAGE_SEVERITY_INFO,
    RHI_MESSAGE_SEVERITY_WARNING,
    RHI_MESSAGE_SEVERITY_ERROR,
    RHI_MESSAGE_SEVERITY_FATAL,
} vri_message_severity_t;

typedef struct {
    void (*message_callback)(vri_message_severity_t severity, const char *message);
} vri_debug_callback_t;

typedef struct {
    void *(*allocate)(size_t size, size_t alignment);
    void (*free)(void *memory, size_t size, size_t alignment);
} vri_allocation_callback_t;

typedef struct {
    uint64_t luid;
    uint32_t device_id;
    vri_vendor_t vendor;
    uint32_t vram;
    uint32_t shared_system_memory;
    vri_gpu_type_t type;
    uint32_t queue_count[VRI_QUEUE_TYPE_COUNT];
} vri_adapter_desc_t;

typedef struct vri_device_base vri_device_t;

struct vri_queue;
struct vri_swapchain;
struct vri_swapchain_desc;

// ==========================================================
// INTERFACES
// ==========================================================
typedef struct {
    void (*device_destroy)(vri_device_t *device);
    bool (*queue_submit)(struct vri_queue *queue);
} vri_core_interface_t;

typedef struct {
    bool (*create_swapchain)(vri_device_t *device, const struct vri_swapchain_desc *desc, struct vri_swapchain **out_swapchain);
    void (*destroy_swapchain)(struct vri_swapchain *swapchain);
} vri_swapchain_interface_t;

// ==========================================================
// DEVICE
// ==========================================================
typedef struct {
    vri_api_t api;
    vri_adapter_desc_t adapter_desc;

    vri_debug_callback_t debug_callback;
    vri_allocation_callback_t allocation_callback;

    bool enable_api_validation;
} vri_device_desc_t;

struct vri_device_base {
    vri_api_t api;
    vri_debug_callback_t debug_callback;
    vri_allocation_callback_t allocation_callback;

    vri_adapter_desc_t adapter_desc;
    vri_core_interface_t core_interface;
};

bool vri_enumerate_adapters(vri_adapter_desc_t *adapter_descs, uint32_t *adapter_desc_count);
bool vri_device_create(const vri_device_desc_t *desc, vri_device_t **device);
void vri_device_destroy(vri_device_t *device);

// ==========================================================
// SWAPCHAIN
// ==========================================================
typedef enum {
    VRI_SWAPCHAIN_FORMAT_REC709_8BIT_SRGB,
    VRI_SWAPCHAIN_FORMAT_REC709_16BIT_LINEAR,
} vri_swapchain_format_t;

typedef struct vri_window_win32 {
    void *hwnd;
} vri_window_win32_t;

typedef struct vri_window_xcb {
    void *connection;
    void *window;
} vri_window_xcb_t;

typedef struct vri_window_wl {
    void *display;
    void *surface;
} vri_window_wl_t;

typedef struct vri_window_metal {
    void *caMetalLayer;
} vri_window_metal_t;

typedef union {
    vri_window_win32_t win32;
    vri_window_xcb_t xcb;
    vri_window_wl_t wl;
    vri_window_metal_t metal;
} vri_window_t;

typedef struct vri_swapchain_desc {
    vri_window_t window;
    uint32_t width;
    uint32_t height;
    vri_swapchain_format_t format;
    uint8_t flags;
    uint8_t frames_in_flight;
} vri_swapchain_desc_t;

typedef struct vri_swapchain {
    void *internal;
} vri_swapchain_t;

typedef struct vri_queue vri_queue_t;                   // Immediate (in d3d11)
typedef struct vri_command_buffer vri_command_buffer_t; // Deferred (in d3d11)

typedef struct vri_buffer vri_buffer_t;
typedef struct vri_texture vri_texture_t;

typedef struct vri_descriptor vri_descriptor_t;
typedef struct vri_pipeline vri_pipeline_t;

typedef struct vri_command_pool vri_command_pool_t;
typedef struct vri_fence vri_fence_t;
