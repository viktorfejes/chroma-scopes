#include "waveform.h"

#include "logger.h"
#include "renderer.h"
#include "texture.h"

#include <assert.h>

#define WF_INT_RES_X 1024
#define WF_INT_RES_Y 512

bool waveform_setup(waveform_t *wf, struct renderer *renderer) {
    ID3D11Device1 *device = renderer->device;

    // Create necessary textures
    {
        const texture_desc_t composite_tex_desc = {
            .width = WF_INT_RES_X,
            .height = WF_INT_RES_Y,
            .format = DXGI_FORMAT_R8G8B8A8_UNORM,
            .array_size = 1,
            .bind_flags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
            .mip_levels = 1,
            .msaa_samples = 1,
            .generate_srv = true,
        };

        if (!texture_create(device, &composite_tex_desc, &wf->composite_tex)) {
            LOG("Failed to create texture for waveform");
            return false;
        }

        LOG("Waveform and RGB Parade textures created");
    }

    // Set up structured buffer fo accumulation
    {
        struct buffer_data { uint32_t r, g, b; };

        D3D11_BUFFER_DESC buffer_desc = {
            .Usage = D3D11_USAGE_DEFAULT,
            .ByteWidth = sizeof(struct buffer_data) * WF_INT_RES_X * WF_INT_RES_Y,
            .BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE,
            .MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
            .StructureByteStride = sizeof(struct buffer_data),
        };

        HRESULT hr = device->lpVtbl->CreateBuffer(device, &buffer_desc, NULL, &wf->accum_buffer);
        if (FAILED(hr)) {
            LOG("Failed to create structured buffer for Waveform");
            return false;
        }

        D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {
            .Format = DXGI_FORMAT_UNKNOWN,
            .ViewDimension = D3D11_UAV_DIMENSION_BUFFER,
            .Buffer = {
                .FirstElement = 0,
                .NumElements = WF_INT_RES_X * WF_INT_RES_Y,
            },
        };

        hr = device->lpVtbl->CreateUnorderedAccessView(device, (ID3D11Resource *)wf->accum_buffer, &uav_desc, &wf->accum_uav);
        if (FAILED(hr)) {
            LOG("Failed to create UAV for Waveform's Structured Buffer");
            return false;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {
            .Format = DXGI_FORMAT_UNKNOWN,
            .ViewDimension = D3D11_SRV_DIMENSION_BUFFER,
            .Buffer = {
                .FirstElement = 0,
                .NumElements = WF_INT_RES_X * WF_INT_RES_Y,
            },
        };

        hr = device->lpVtbl->CreateShaderResourceView(device, (ID3D11Resource *)wf->accum_buffer, &srv_desc, &wf->accum_srv);
        if (FAILED(hr)) {
            LOG("Failed to create UAV for Waveform's Structured Buffer");
            return false;
        }
    }

    return true;
}

void waveform_render(waveform_t *wf, struct renderer *renderer, texture_t *capture_texture) {
    ID3D11DeviceContext1 *context = renderer->context;
    unsigned int clear_color_uint[4] = {0, 0, 0, 0};
    float clear_color_float[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    ID3D11UnorderedAccessView *nulluav = NULL;
    uint32_t thread_groups[] = {8, 8, 1};

    // 1. Accumulate samples
    shader_pipeline_bind(context, &renderer->passes.wf_accum);
    context->lpVtbl->CSSetSamplers(context, 0, 1, &renderer->sampler_states[SAMPLER_LINEAR_CLAMP]);
    context->lpVtbl->CSSetShaderResources(context, 0, 1, &capture_texture->srv);
    context->lpVtbl->ClearUnorderedAccessViewUint(context, wf->accum_uav, clear_color_uint);
    context->lpVtbl->CSSetUnorderedAccessViews(context, 0, 1, &wf->accum_uav, NULL);
    context->lpVtbl->Dispatch(
        context,
        (capture_texture->width + (thread_groups[0] - 1)) / thread_groups[0],
        (capture_texture->width + (thread_groups[1] - 1)) / thread_groups[1],
        thread_groups[2]);
    context->lpVtbl->CSSetUnorderedAccessViews(context, 0, 1, &nulluav, NULL);

    // 3. Composite with overlay into final texture
    shader_pipeline_bind(context, &renderer->passes.wf_comp);
    context->lpVtbl->CSSetShaderResources(context, 0, 1, &wf->accum_srv);
    context->lpVtbl->ClearUnorderedAccessViewFloat(context, wf->composite_tex.uav[0], clear_color_float);
    context->lpVtbl->CSSetUnorderedAccessViews(context, 0, 1, &wf->composite_tex.uav[0], NULL);
    context->lpVtbl->Dispatch(
        context,
        (wf->composite_tex.width + (thread_groups[0] - 1)) / thread_groups[0],
        (wf->composite_tex.width + (thread_groups[1] - 1)) / thread_groups[1],
        thread_groups[2]);
    context->lpVtbl->CSSetUnorderedAccessViews(context, 0, 1, &nulluav, NULL);

}

texture_t *waveform_get_texture(waveform_t *wf) {
    assert(wf);
    return &wf->composite_tex;
}
