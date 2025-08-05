#pragma once

#include "capture.h"
#include "shader.h"
#include "texture.h"
#include "vectorscope.h"
#include "waveform.h"

#include <stdbool.h>

#include <d3d11_1.h>
#include <dxgi1_4.h>

struct ui_state;
struct ui_element;
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
    shader_t unit_quad_vs;
    shader_t vectorscope_cs;
    shader_t vectorscope_cs1;
    shader_t vectorscope_blur_cs;
    shader_t composite_ps;
    shader_t ui_ps;

    shader_t vs_accum_cs;
    shader_t vs_blur_cs;
    shader_t vs_comp_cs;

    shader_t wf_accum_cs;
    shader_t wf_comp_cs;
    shader_t parade_comp_cs;
};

struct passes {
    shader_pipeline_t vs_accum;
    shader_pipeline_t vs_blur;
    shader_pipeline_t vs_comp;

    shader_pipeline_t wf_accum;
    shader_pipeline_t wf_comp;
    shader_pipeline_t parade_comp;

    shader_pipeline_t vectorscope;
    shader_pipeline_t vectorscope1;
    shader_pipeline_t vectorscope_blur;
    shader_pipeline_t composite;
    shader_pipeline_t ui;
};

typedef struct renderer {
    ID3D11Device1 *device;
    ID3D11DeviceContext1 *context;
    D3D_FEATURE_LEVEL feature_level;
    swapchain_t swapchain;
    swapchain_t overlay_swapchain;
    ID3DUserDefinedAnnotation *annotation;

    capture_t capture;
    texture_t blit_texture;
    texture_t vectorscope_texture;
    texture_t vectorscope_buckets;
    texture_t vectorscope_float;
    texture_t vectorscope_out;
    texture_t ui_rt;
    texture_t default_white_px;

    // States
    ID3D11RasterizerState *rasterizer_states[RASTER_STATE_COUNT];
    ID3D11BlendState *blend_states[BLEND_STATE_COUNT];
    ID3D11SamplerState *sampler_states[SAMPLER_STATE_COUNT];

    // Scope modules
    vectorscope_t vectorscope;
    waveform_t waveform;

    // Shaders and pipelines
    struct shaders shaders;
    struct passes passes;

    // Buffers
    ID3D11Buffer *per_frame_buffer;
    ID3D11Buffer *per_ui_mesh_buffer;

    struct window *window;
} renderer_t;

bool renderer_initialize(struct window *window, renderer_t *out_renderer);
void renderer_terminate(renderer_t *renderer);
void renderer_begin_frame(renderer_t *renderer);
void renderer_end_frame(renderer_t *renderer);

bool renderer_overlay_swapchain_create(renderer_t *renderer, struct window *window);
void renderer_overlay_swapchain_destroy(renderer_t *renderer);
void renderer_overlay_begin_frame(renderer_t *renderer);
void renderer_draw_overlay(renderer_t *renderer);
void renderer_overlay_end_frame(renderer_t *renderer);

void renderer_draw_scopes(renderer_t *renderer);
void renderer_calculate_vectorscope(renderer_t *renderer, const texture_t* in_texture, texture_t *out_texture);
void renderer_calculate_waveform(renderer_t *renderer, const texture_t *in_texture, texture_t *out_texture);
void renderer_calculate_histogram(renderer_t *renderer, const texture_t *in_texture, texture_t *out_texture);

void renderer_draw_ui(renderer_t *renderer, struct ui_state *ui_state, struct ui_element *root, bool debug_view);
void renderer_draw_composite(renderer_t *renderer);

void check_d3d11_debug_messages(ID3D11Device *device);
