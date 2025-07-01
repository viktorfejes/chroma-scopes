#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <d3d11_1.h>
#include <dxgi1_4.h>

#define MAX_MIP_LEVELS 16

typedef enum texture_format {
    TEXTURE_FORMAT_LDR_SRGB,
    TEXTURE_FORMAT_LDR_RAW,
    TEXTURE_FORMAT_HDR_RAW
} texture_format_t;

typedef struct texture_desc {
    uint16_t width;
    uint16_t height;
    DXGI_FORMAT format;
    uint32_t bind_flags;
    void *data;
    uint32_t row_pitch;
    uint32_t array_size;
    uint32_t mip_levels;
    uint32_t msaa_samples;
    bool generate_srv;
    bool is_cubemap;
} texture_desc_t;

typedef struct texture {
    int16_t width;
    int16_t height;
    DXGI_FORMAT format;
    uint32_t bind_flags;
    uint32_t mip_levels;
    uint32_t array_size;
    uint32_t msaa_samples;
    bool is_cubemap;
    bool has_srv;
    ID3D11Texture2D *texture;
    ID3D11ShaderResourceView *srv;
    ID3D11RenderTargetView *rtv[6];
    ID3D11DepthStencilView *dsv;
    ID3D11UnorderedAccessView *uav[MAX_MIP_LEVELS];
} texture_t;

bool texture_load(ID3D11Device1 *device, const char *filename, texture_format_t format, texture_t *out_texture);
bool texture_create(ID3D11Device1 *device, const texture_desc_t *desc, texture_t *out_texture);
bool texture_create_from_backbuffer(ID3D11Device1 *device, IDXGISwapChain3 *swapchain, texture_t *out_texture);
void texture_destroy(texture_t *texture);
bool texture_resize(ID3D11Device1 *device, texture_t *texture, uint16_t w, uint16_t h);
