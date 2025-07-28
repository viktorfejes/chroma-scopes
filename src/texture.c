#include "texture.h"

#include "logger.h"
#include "macros.h"

#include <assert.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

struct format_binding_info {
    DXGI_FORMAT texture_format;
    DXGI_FORMAT dsv_format;
    DXGI_FORMAT srv_format;
};

typedef void *(*texture_load_fn)(char const *filename, int *w, int *h, int *channels_in_file, int desired_channels);

struct texture_handle {
    DXGI_FORMAT format;
    texture_load_fn load;
    uint32_t bpp; // bytes per pixel
};

static const struct texture_handle texture_handlers[] = {
    [TEXTURE_FORMAT_LDR_SRGB] = {
        .format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        .load = (texture_load_fn)stbi_load,
        .bpp = 4,
    },
    [TEXTURE_FORMAT_LDR_RAW] = {
        .format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .load = (texture_load_fn)stbi_load,
        .bpp = 4,
    },
    [TEXTURE_FORMAT_HDR_RAW] = {
        .format = DXGI_FORMAT_R32G32B32A32_FLOAT,
        .load = (texture_load_fn)stbi_loadf,
        .bpp = 4 * sizeof(float),
    },
};

static struct format_binding_info get_format_binding_info(DXGI_FORMAT format);

bool texture_load(ID3D11Device1 *device, const char *filename, texture_format_t format, texture_t *out_texture) {
    assert(out_texture && "Output texture cannot be NULL");

    int w, h, c;
    void *image_data = texture_handlers[format].load(filename, &w, &h, &c, 4);
    if (!image_data) {
        LOG("Couldn't load texture from disc: %s", filename);
        return false;
    }

    // Create texture with data on GPU
    const texture_desc_t desc = {
        .width = w,
        .height = h,
        .format = texture_handlers[format].format,
        .bind_flags = D3D11_BIND_SHADER_RESOURCE,
        .data = image_data,
        .row_pitch = texture_handlers[format].bpp * w,
        .array_size = 1,
        .mip_levels = 1,
        .msaa_samples = 1,
        .generate_srv = true,
        .is_cubemap = false,
    };

    if (!texture_create(device, &desc, out_texture)) {
        LOG("Couldn't create texture on GPU");
        return false;
    }

    stbi_image_free(image_data);

    return true;
}

bool texture_create(ID3D11Device1 *device, const texture_desc_t *desc, texture_t *out_texture) {
    assert(out_texture && "Output texture cannot be NULL");
    assert(desc && "Texture Description cannot be NULL");

    if (desc->msaa_samples > 1) {
        // Multi-sampled textures cannot have mipmaps
        if (desc->mip_levels > 1) {
            LOG("MSAA textures can't have mipmaps");
            return false;
        }

        // UAVs are not supported on multisampled textures
        if (desc->bind_flags & D3D11_BIND_UNORDERED_ACCESS) {
            LOG("UAV's are not supported on multisampled textures");
            return false;
        }
    }

    // Special handling for depth textures that need SRV bindings
    bool depth_srv = (desc->bind_flags & D3D11_BIND_DEPTH_STENCIL) && desc->generate_srv;
    struct format_binding_info depth_srv_format = get_format_binding_info(desc->format);

    D3D11_TEXTURE2D_DESC gpu_desc = {
        .Width = desc->width,
        .Height = desc->height,
        .MipLevels = desc->mip_levels,
        .ArraySize = desc->array_size,
        .Format = depth_srv ? depth_srv_format.texture_format : desc->format,
        .SampleDesc = {
            .Count = 1,
            .Quality = 0},
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = desc->bind_flags};

    // Add extra flag for cubemap format
    if (desc->is_cubemap) {
        gpu_desc.MiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;
    }

    // Check MSAA availability
    UINT quality_levels = 0;
    if (desc->msaa_samples > 1) {
        device->lpVtbl->CheckMultisampleQualityLevels(device, gpu_desc.Format, desc->msaa_samples, &quality_levels);
        if (quality_levels == 0) {
            LOG("MSAA %ux not supported for format", desc->msaa_samples);
            return false;
        }
        // Modify GPU texture description
        gpu_desc.SampleDesc.Count = desc->msaa_samples;
        gpu_desc.SampleDesc.Quality = quality_levels - 1;
    }

    // Initial data
    D3D11_SUBRESOURCE_DATA init_data = {0};
    D3D11_SUBRESOURCE_DATA *data_ptr = NULL;
    if (desc->data && desc->row_pitch > 0) {
        init_data.pSysMem = desc->data;
        init_data.SysMemPitch = desc->row_pitch;
        data_ptr = &init_data;
    }

    // Create the texture
    HRESULT hr = device->lpVtbl->CreateTexture2D(device, &gpu_desc, data_ptr, &out_texture->texture);
    if (FAILED(hr)) {
        LOG("Failed to create Texture2D. HRESULT: 0x%lX.", hr);
        return false;
    }

    if (desc->generate_srv && (desc->bind_flags & D3D11_BIND_SHADER_RESOURCE)) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {
            .Format = depth_srv ? depth_srv_format.srv_format : desc->format,
        };

        if (desc->is_cubemap) {
            srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
            srv_desc.TextureCube.MipLevels = desc->mip_levels;
            srv_desc.TextureCube.MostDetailedMip = 0;
        } else if (desc->array_size > 1) {
            srv_desc.ViewDimension = (desc->msaa_samples > 1) ? D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY : D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
            srv_desc.Texture2DArray.MipLevels = desc->mip_levels;
            srv_desc.Texture2DArray.ArraySize = desc->array_size;
            srv_desc.Texture2DArray.FirstArraySlice = 0;
            srv_desc.Texture2DArray.MostDetailedMip = 0;
        } else {
            srv_desc.ViewDimension = (desc->msaa_samples > 1) ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Texture2D.MipLevels = desc->mip_levels;
            srv_desc.Texture2D.MostDetailedMip = 0;
        }

        // Create the SRV
        hr = device->lpVtbl->CreateShaderResourceView(device, (ID3D11Resource *)out_texture->texture, &srv_desc, &out_texture->srv);
        if (FAILED(hr)) {
            LOG("Failed to create Shader Resource View for texture.");
            return false;
        }
    }

    if (desc->bind_flags & D3D11_BIND_UNORDERED_ACCESS) {
        for (uint32_t mip = 0; mip < desc->mip_levels; ++mip) {
            D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {
                .Format = gpu_desc.Format,
            };

            if (desc->is_cubemap) {
                uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
                uav_desc.Texture2DArray.MipSlice = mip;
                uav_desc.Texture2DArray.FirstArraySlice = 0;
                uav_desc.Texture2DArray.ArraySize = 6;
            } else if (desc->array_size > 1) {
                uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
                uav_desc.Texture2DArray.MipSlice = mip;
                uav_desc.Texture2DArray.FirstArraySlice = 0;
                uav_desc.Texture2DArray.ArraySize = desc->array_size;
            } else {
                uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
                uav_desc.Texture2D.MipSlice = mip;
            }

            // Create the UAV
            hr = device->lpVtbl->CreateUnorderedAccessView(device, (ID3D11Resource *)out_texture->texture, &uav_desc, &out_texture->uav[mip]);
            if (FAILED(hr)) {
                LOG("Failed to create Unordered Access View for texture.");
                return false;
            }
        }
    }

    if (desc->bind_flags & D3D11_BIND_RENDER_TARGET) {
        for (uint32_t i = 0; i < desc->array_size; ++i) {
            D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {
                .Format = gpu_desc.Format,
            };

            if (desc->array_size > 1) {
                rtv_desc.ViewDimension = (desc->msaa_samples > 1) ? D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY : D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                rtv_desc.Texture2DArray.MipSlice = 0;
                rtv_desc.Texture2DArray.ArraySize = 1;
                rtv_desc.Texture2DArray.FirstArraySlice = i;
            } else {
                rtv_desc.ViewDimension = (desc->msaa_samples > 1) ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
                rtv_desc.Texture2D.MipSlice = 0;
            }

            // Create the RTV
            hr = device->lpVtbl->CreateRenderTargetView(device, (ID3D11Resource *)out_texture->texture, &rtv_desc, &out_texture->rtv[i]);
            if (FAILED(hr)) {
                LOG("Failed to create Render Target View for texture.");
                return false;
            }
        }
    }

    if (desc->bind_flags & D3D11_BIND_DEPTH_STENCIL) {
        D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc = {
            .Format = depth_srv ? depth_srv_format.dsv_format : gpu_desc.Format,
            .ViewDimension = (desc->msaa_samples > 1) ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D,
        };

        // Create the DSV
        hr = device->lpVtbl->CreateDepthStencilView(device, (ID3D11Resource *)out_texture->texture, &dsv_desc, &out_texture->dsv);
        if (FAILED(hr)) {
            LOG("Failed to create Depth Stencil View for texture.");
            return false;
        }
    }

    out_texture->width = desc->width;
    out_texture->height = desc->height;
    out_texture->format = desc->format;
    out_texture->mip_levels = desc->mip_levels;
    out_texture->array_size = desc->array_size;
    out_texture->is_cubemap = desc->is_cubemap;
    out_texture->bind_flags = desc->bind_flags;
    out_texture->has_srv = desc->generate_srv;
    out_texture->msaa_samples = desc->msaa_samples;

    return true;
}

bool texture_create_from_backbuffer(ID3D11Device1 *device, IDXGISwapChain3 *swapchain, texture_t *out_texture) {
    assert(device && "ID3D11Device1 cannot be NULL");
    assert(swapchain && "IDXGISwapChain3 cannot be NULL");
    assert(out_texture && "Output texture cannot be NULL");

    ID3D11Texture2D *backbuffer = NULL;
    HRESULT hr = swapchain->lpVtbl->GetBuffer(swapchain, 0, IID_PPV_ARGS_C(ID3D11Texture2D, &backbuffer));
    if (FAILED(hr)) {
        LOG("Failed to create RTV for the backbuffer");
        return false;
    }

    // Create rendertarget view from this
    hr = device->lpVtbl->CreateRenderTargetView(device, (ID3D11Resource *)backbuffer, NULL, &out_texture->rtv[0]);
    if (FAILED(hr)) {
        LOG("Failed to create RTV from the backbuffer");
        return false;
    }

    // Get the backbuffer's texture description to populate our internal description
    D3D11_TEXTURE2D_DESC desc = {0};
    backbuffer->lpVtbl->GetDesc(backbuffer, &desc);

    out_texture->width = desc.Width;
    out_texture->height = desc.Height;
    out_texture->format = desc.Format;
    out_texture->mip_levels = desc.MipLevels;
    out_texture->array_size = desc.ArraySize;
    out_texture->has_srv = false; // Not creating one now
    out_texture->msaa_samples = desc.SampleDesc.Count;
    out_texture->is_cubemap = false; // Backbuffers are never cubemaps
    out_texture->bind_flags = desc.BindFlags;

    LOG("Texture (%dx%d) successfully created from backbuffer", out_texture->width, out_texture->height);

    // Release the backbuffer
    backbuffer->lpVtbl->Release(backbuffer);

    return true;
}

bool texture_create_from_data(ID3D11Device1 *device, uint8_t *data, uint16_t width, uint16_t height, texture_t *out_texture) {
    assert(out_texture && "Output texture cannot be NULL");
    assert(data);

    // Create texture with data on GPU
    const texture_desc_t desc = {
        .width = width,
        .height = height,
        .format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .bind_flags = D3D11_BIND_SHADER_RESOURCE,
        .data = data,
        .row_pitch = 4 * width,
        .array_size = 1,
        .mip_levels = 1,
        .msaa_samples = 1,
        .generate_srv = true,
        .is_cubemap = false,
    };

    if (!texture_create(device, &desc, out_texture)) {
        LOG("Couldn't create texture on GPU");
        return false;
    }

    return true;
}

void texture_destroy(texture_t *texture) {
    if (texture) {
        if (texture->srv) {
            texture->srv->lpVtbl->Release(texture->srv);
            texture->srv = NULL;
        }

        if (texture->dsv) {
            texture->dsv->lpVtbl->Release(texture->dsv);
            texture->dsv = NULL;
        }

        uint32_t rtv_count = texture->is_cubemap ? 6 : 1;
        for (uint32_t i = 0; i < rtv_count; ++i) {
            if (texture->rtv[i]) {
                texture->rtv[i]->lpVtbl->Release(texture->rtv[i]);
                texture->rtv[i] = NULL;
            }
        }

        for (uint32_t i = 0; i < texture->mip_levels; ++i) {
            if (texture->uav[i]) {
                texture->uav[i]->lpVtbl->Release(texture->uav[i]);
                texture->uav[i] = NULL;
            }
        }

        if (texture->texture) {
            texture->texture->lpVtbl->Release(texture->texture);
            texture->texture = NULL;
        }

        memset(texture, 0, sizeof(texture_t));
    }
}

bool texture_resize(ID3D11Device1 *device, texture_t *texture, uint16_t w, uint16_t h) {
    // Create texture description before destroy
    const texture_desc_t desc = {
        .width = w,
        .height = h,
        .format = texture->format,
        .array_size = texture->array_size,
        .bind_flags = texture->bind_flags,
        .mip_levels = texture->mip_levels,
        .msaa_samples = texture->msaa_samples,
        .row_pitch = texture_handlers[texture->format].bpp * w,
        .generate_srv = texture->has_srv,
        .is_cubemap = texture->is_cubemap,
    };

    // Destroy texture internals (weird?)
    texture_destroy(texture);

    // Recreate the texture with the saved description/new size
    if (!texture_create(device, &desc, texture)) {
        return false;
    }

    return true;
}

static struct format_binding_info get_format_binding_info(DXGI_FORMAT format) {
    switch (format) {
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
            return (struct format_binding_info){DXGI_FORMAT_R24G8_TYPELESS, DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_R24_UNORM_X8_TYPELESS};
        case DXGI_FORMAT_D32_FLOAT:
            return (struct format_binding_info){DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_R32_FLOAT};
        default:
            return (struct format_binding_info){format, format, format};
    }
}
