#pragma once

#include "capture.h"
#include "shader.h"
#include "texture.h"
#include "ui.h"

#include <stdbool.h>

#include <d3d11_1.h>
#include <dxgi1_4.h>

struct ui_state;
struct window;

typedef struct swapchain {
    IDXGISwapChain3 *swapchain;
    texture_t *texture;
} swapchain_t;

enum rasterizer_state {
    RASTER_2D_DEFAULT,
    RASTER_2D_SCISSOR,
    RASTER_2D_WIREFRAME,
    RASTER_STATE_COUNT
};

enum blend_state {
    BLEND_OPAQUE,
    BLEND_ALPHA,
    BLEND_ADDITIVE,
    BLEND_MULTIPLY,
    BLEND_PREMULT_ALPHA,
    BLEND_STATE_COUNT
};

enum sampler_state {
    SAMPLER_LINEAR_WRAP,
    SAMPLER_LINEAR_CLAMP,
    SAMPLER_POINT_WRAP,
    SAMPLER_POINT_CLAMP,
    SAMPLER_ANISOTROPIC_CLAMP,
    SAMPLER_STATE_COUNT
};

struct shaders {
    shader_t fs_triangle_vs;
    shader_t vectorscope_cs;
    shader_t composite_ps;
};

struct passes {
    shader_pipeline_t vectorscope;
    shader_pipeline_t composite;
};

typedef struct renderer {
    ID3D11Device1 *device;
    ID3D11DeviceContext1 *context;
    D3D_FEATURE_LEVEL feature_level;
    swapchain_t swapchain;
    ID3DUserDefinedAnnotation *annotation;

    capture_t capture;
    texture_t blit_texture;
    texture_t vectorscope_texture;

    // UI
    ui_state_t ui_state;

    // States
    ID3D11RasterizerState *rasterizer_states[RASTER_STATE_COUNT];
    ID3D11BlendState *blend_states[BLEND_STATE_COUNT];
    ID3D11SamplerState *sampler_states[SAMPLER_STATE_COUNT];

    // Shaders and pipelines
    struct shaders shaders;
    struct passes passes;
} renderer_t;

bool renderer_initialize(struct window *window, renderer_t *out_renderer);
void renderer_terminate(renderer_t *renderer);
void renderer_begin_frame(renderer_t *renderer);
void renderer_draw(renderer_t *renderer);
void renderer_end_frame(renderer_t *renderer);

void check_d3d11_debug_messages(ID3D11Device *device);
