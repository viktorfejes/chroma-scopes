#include "vectorscope.h"

#include "logger.h"
#include "renderer.h"
#include "texture.h"

#include <assert.h>
#include <string.h>

#define VS_INT_RES 1024

struct vs_cbuffer {
    float2_t resolution;
    float padding[2];
};

bool vectorscope_setup(vectorscope_t *vs, struct renderer *renderer) {
    ID3D11Device1 *device = renderer->device;

    // Create necessary textures
    {
        const texture_desc_t accum_tex_desc = {
            .width = VS_INT_RES,
            .height = VS_INT_RES,
            .format = DXGI_FORMAT_R32_UINT,
            .array_size = 1,
            .bind_flags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
            .mip_levels = 1,
            .msaa_samples = 1,
            .generate_srv = true,
        };

        if (!texture_create(device, &accum_tex_desc, &vs->accum_tex)) {
            LOG("Failed to create texture for vectorscope");
            return false;
        }

        const texture_desc_t blur_tex_desc = {
            .width = VS_INT_RES,
            .height = VS_INT_RES,
            .format = DXGI_FORMAT_R32_FLOAT,
            .array_size = 1,
            .bind_flags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
            .mip_levels = 1,
            .msaa_samples = 1,
            .generate_srv = true,
        };

        if (!texture_create(device, &blur_tex_desc, &vs->blur_tex)) {
            LOG("Failed to create texture for vectorscope");
            return false;
        }

        const texture_desc_t composite_tex_desc = {
            .width = 1024,
            .height = 576,
            .format = DXGI_FORMAT_R8G8B8A8_UNORM,
            .array_size = 1,
            .bind_flags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
            .mip_levels = 1,
            .msaa_samples = 1,
            .generate_srv = true,
        };

        if (!texture_create(device, &composite_tex_desc, &vs->composite_tex)) {
            LOG("Failed to create texture for vectorscope");
            return false;
        }

        LOG("Vectorscope textures created");
    }

    // Create needed constant buffers
    {
        D3D11_BUFFER_DESC desc = {
            .Usage = D3D11_USAGE_DYNAMIC,
            .ByteWidth = sizeof(struct vs_cbuffer),
            .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
            .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
        };

        HRESULT hr = device->lpVtbl->CreateBuffer(device, &desc, NULL, &vs->cbuffer);
        if (FAILED(hr)) {
            LOG("Failed to create constant buffer for vectorscope");
            return false;
        }

        LOG("Vectorscope constant buffers created");
    }

    return true;
}

void vectorscope_render(vectorscope_t *vs, struct renderer *renderer, texture_t *capture_texture) {
    ID3D11DeviceContext1 *context = renderer->context;
    unsigned int clear_color_uint[4] = {0, 0, 0, 0};
    float clear_color_float[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    ID3D11UnorderedAccessView *nulluav = NULL;
    uint32_t thread_groups[] = {8, 8, 1};

    // 1. Accumulate samples
    shader_pipeline_bind(context, &renderer->passes.vs_accum);
    context->lpVtbl->CSSetSamplers(context, 0, 1, &renderer->sampler_states[SAMPLER_LINEAR_CLAMP]);
    context->lpVtbl->CSSetShaderResources(context, 0, 1, &capture_texture->srv);
    context->lpVtbl->ClearUnorderedAccessViewUint(context, vs->accum_tex.uav[0], clear_color_uint);
    context->lpVtbl->CSSetUnorderedAccessViews(context, 0, 1, &vs->accum_tex.uav[0], NULL);
    context->lpVtbl->Dispatch(
        context,
        (capture_texture->width + (thread_groups[0] - 1)) / thread_groups[0],
        (capture_texture->width + (thread_groups[1] - 1)) / thread_groups[1],
        thread_groups[2]);
    context->lpVtbl->CSSetUnorderedAccessViews(context, 0, 1, &nulluav, NULL);

    // 2. Blur samples
    shader_pipeline_bind(context, &renderer->passes.vs_blur);
    context->lpVtbl->CSSetShaderResources(context, 0, 1, &vs->accum_tex.srv);
    context->lpVtbl->ClearUnorderedAccessViewFloat(context, vs->blur_tex.uav[0], clear_color_float);
    context->lpVtbl->CSSetUnorderedAccessViews(context, 0, 1, &vs->blur_tex.uav[0], NULL);
    context->lpVtbl->Dispatch(
        context,
        (vs->blur_tex.width + (thread_groups[0] - 1)) / thread_groups[0],
        (vs->blur_tex.width + (thread_groups[1] - 1)) / thread_groups[1],
        thread_groups[2]);
    context->lpVtbl->CSSetUnorderedAccessViews(context, 0, 1, &nulluav, NULL);

    // 3. Composite with overlay into final texture
    shader_pipeline_bind(context, &renderer->passes.vs_comp);
    context->lpVtbl->CSSetShaderResources(context, 0, 1, &vs->blur_tex.srv);
    context->lpVtbl->ClearUnorderedAccessViewFloat(context, vs->composite_tex.uav[0], clear_color_float);
    context->lpVtbl->CSSetUnorderedAccessViews(context, 0, 1, &vs->composite_tex.uav[0], NULL);

    struct vs_cbuffer cb = {
        .resolution = (float2_t){vs->composite_tex.width, vs->composite_tex.height},
    };
    D3D11_MAPPED_SUBRESOURCE map;
    context->lpVtbl->Map(context, (ID3D11Resource *)vs->cbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
    memcpy(map.pData, &cb, sizeof(struct vs_cbuffer));
    context->lpVtbl->Unmap(context, (ID3D11Resource *)vs->cbuffer, 0);
    context->lpVtbl->CSSetConstantBuffers(context, 0, 1, &vs->cbuffer);

    context->lpVtbl->Dispatch(
        context,
        (vs->composite_tex.width + (thread_groups[0] - 1)) / thread_groups[0],
        (vs->composite_tex.width + (thread_groups[1] - 1)) / thread_groups[1],
        thread_groups[2]);
    context->lpVtbl->CSSetUnorderedAccessViews(context, 0, 1, &nulluav, NULL);
}

texture_t *vectorscope_get_texture(vectorscope_t *vs) {
    assert(vs);
    return &vs->composite_tex;
}
