#include "renderer.h"

#include "capture.h"
#include "logger.h"
#include "macros.h"
#include "math.h"
#include "shader.h"
#include "ui.h"
#include "window.h"

#include <assert.h>
#include <stdlib.h>

#include <d3d11_1.h>

struct per_frame_data {
    float4x4_t projection;
};

struct per_ui_mesh_data {
    float2_t position;
    float2_t size;
    float2_t uv_offset;
    float2_t uv_scale;
    float4_t color;
};

static bool create_device(ID3D11Device1 **device, ID3D11DeviceContext1 **context, D3D_FEATURE_LEVEL *feature_level);
static bool create_swapchain(ID3D11Device1 *device, HWND hwnd, texture_t *swapchain_texture, IDXGISwapChain3 **swapchain);
static void destroy_swapchain(swapchain_t *swapchain);

static bool create_pipeline_states(renderer_t *renderer);
static bool create_textures(renderer_t *renderer);
static bool create_shader_pipelines(renderer_t *renderer);
static bool create_constant_buffers(renderer_t *renderer);

bool renderer_initialize(window_t *window, renderer_t *out_renderer) {
    assert(out_renderer && "Renderer pointer MUST NOT be NULL");

    // Store a pointer to the window for convenience
    out_renderer->window = window;

    if (!create_device(&out_renderer->device, &out_renderer->context, &out_renderer->feature_level)) {
        LOG("Failed to create D3D11 device");
        return false;
    }
    LOG("D3D11 Device created");

    // TODO: Modify the create swapchain by only passing in a swapchain struct
    // and then it can do malloc inside the function and this will be cleaned up
    out_renderer->swapchain.texture = (texture_t *)malloc(sizeof(texture_t));
    memset(out_renderer->swapchain.texture, 0, sizeof(texture_t));
    if (!create_swapchain(out_renderer->device, window->hwnd, out_renderer->swapchain.texture, &out_renderer->swapchain.swapchain)) {
        LOG("Failed to create D3D11 Swapchain");
        return false;
    }
    LOG("D3D11 Swapchain created");

#ifdef _DEBUG
    // Grab the annotation interface for debugging purposes
    HRESULT hr = out_renderer->context->lpVtbl->QueryInterface(out_renderer->context, IID_PPV_ARGS_C(ID3DUserDefinedAnnotation, &out_renderer->annotation));
    if (FAILED(hr)) {
        LOG("Failed to get the annotation interface");
        // Not returning false here, just moving on
    }
    LOG("D3D11 Annotation interface was successfully queried");
#endif

    // Initialize capture interface
    if (!capture_initialize(out_renderer->device, &out_renderer->capture)) {
        LOG("Failed to initialize capture interface");
        return false;
    }
    LOG("DXGI Capture interface initialized");

    // Create the states we will need
    if (!create_pipeline_states(out_renderer)) {
        LOG("Failed to create necessary pipeline states");
        return false;
    }

    // Create textures we will be using
    if (!create_textures(out_renderer)) {
        LOG("Failed to create necessary textures");
        return false;
    }

    // Create shader pipelines
    if (!create_shader_pipelines(out_renderer)) {
        LOG("Failed to create necessary shader pipelines");
        return false;
    }

    // Create buffers
    if (!create_constant_buffers(out_renderer)) {
        LOG("Failed to create necessary buffers");
        return false;
    }

    // Setup vectorscope
    if (!vectorscope_setup(&out_renderer->vectorscope, out_renderer)) {
        LOG("Failed to setup vectorscope");
        return false;
    }

    // Setup waveform and parade
    if (!waveform_setup(&out_renderer->waveform, out_renderer)) {
        LOG("Failed to setup waveform");
        return false;
    }

    // Set primitive topology and forget
    out_renderer->context->lpVtbl->IASetPrimitiveTopology(out_renderer->context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    return true;
}

void renderer_terminate(renderer_t *renderer) {
    if (renderer) {
        capture_terminate(&renderer->capture);

        destroy_swapchain(&renderer->swapchain);

        if (renderer->context) renderer->context->lpVtbl->Release(renderer->context);
        renderer->context = NULL;

        if (renderer->device) renderer->device->lpVtbl->Release(renderer->device);
        renderer->device = NULL;
    }
}

void renderer_begin_frame(renderer_t *renderer) {
    ID3D11DeviceContext1 *context = renderer->context;

    // FIXME: I don't think the projection will change ever, so we might only need to
    // update it when resizing but otherwise this can be static.
    D3D11_MAPPED_SUBRESOURCE mapped;
    context->lpVtbl->Map(context, (ID3D11Resource *)renderer->per_frame_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    struct per_frame_data *per_frame_buffer = mapped.pData;
    per_frame_buffer->projection = mat_orthographic_offcenter_lh(0.0f, (float)renderer->window->width, (float)renderer->window->height, 0.0f, 0.0f, 1.0f);
    context->lpVtbl->Unmap(context, (ID3D11Resource *)renderer->per_frame_buffer, 0);

    context->lpVtbl->VSSetConstantBuffers(context, 0, 1, &renderer->per_frame_buffer);
}

void renderer_draw_scopes(renderer_t *renderer) {
    capture_frame(&renderer->capture, (rect_t){0, 0, 500, 500}, renderer->context, &renderer->blit_texture);

    ID3D11DeviceContext1 *context = renderer->context;
    float clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};

    // Generate Vectorscope
    {
        // Bind the shader pipeline
        shader_pipeline_bind(context, &renderer->passes.vectorscope);

        // Bind the sampler
        context->lpVtbl->CSSetSamplers(context, 0, 1, &renderer->sampler_states[SAMPLER_LINEAR_CLAMP]);

        // Bind the Capture Texture as SRV
        context->lpVtbl->CSSetShaderResources(context, 0, 1, &renderer->blit_texture.srv);

        // Clear the UAV
        context->lpVtbl->ClearUnorderedAccessViewFloat(context, renderer->vectorscope_texture.uav[0], clear_color);

        // Bind the UAV
        context->lpVtbl->CSSetUnorderedAccessViews(context, 0, 1, &renderer->vectorscope_texture.uav[0], NULL);

        // Calculate dispatch size
        uint32_t thread_groups[] = {16, 16, 1};
        uint32_t dispatch_x = (renderer->vectorscope_texture.width + (thread_groups[0] - 1)) / thread_groups[0];
        uint32_t dispatch_y = (renderer->vectorscope_texture.width + (thread_groups[1] - 1)) / thread_groups[1];
        uint32_t dispatch_z = thread_groups[2];

        context->lpVtbl->Dispatch(context, dispatch_x, dispatch_y, dispatch_z);

        // Unbind UAV
        ID3D11UnorderedAccessView *nulluav = NULL;
        context->lpVtbl->CSSetUnorderedAccessViews(context, 0, 1, &nulluav, NULL);
    }

    renderer_calculate_vectorscope(renderer, &renderer->blit_texture, &renderer->vectorscope_buckets);
}

void renderer_calculate_vectorscope(renderer_t *renderer, const texture_t *in_texture, texture_t *out_texture) {
    ID3D11DeviceContext1 *context = renderer->context;
    unsigned int clear_color[4] = {0, 0, 0, 0};

    shader_pipeline_bind(context, &renderer->passes.vectorscope1);

    context->lpVtbl->CSSetSamplers(context, 0, 1, &renderer->sampler_states[SAMPLER_LINEAR_CLAMP]);
    context->lpVtbl->CSSetShaderResources(context, 0, 1, &in_texture->srv);

    context->lpVtbl->ClearUnorderedAccessViewUint(context, out_texture->uav[0], clear_color);
    context->lpVtbl->CSSetUnorderedAccessViews(context, 0, 1, &out_texture->uav[0], NULL);

    {
        uint32_t thread_groups[] = {16, 16, 1};
        uint32_t dispatch_x = (out_texture->width + (thread_groups[0] - 1)) / thread_groups[0];
        uint32_t dispatch_y = (out_texture->width + (thread_groups[1] - 1)) / thread_groups[1];
        uint32_t dispatch_z = thread_groups[2];

        context->lpVtbl->Dispatch(context, dispatch_x, dispatch_y, dispatch_z);
    }

    // Unbind UAV
    ID3D11UnorderedAccessView *nulluav = NULL;
    context->lpVtbl->CSSetUnorderedAccessViews(context, 0, 1, &nulluav, NULL);

    shader_pipeline_bind(context, &renderer->passes.vectorscope_blur);
    context->lpVtbl->CSSetShaderResources(context, 0, 1, &out_texture->srv);
    context->lpVtbl->ClearUnorderedAccessViewUint(context, renderer->vectorscope_float.uav[0], clear_color);
    context->lpVtbl->CSSetUnorderedAccessViews(context, 0, 1, &renderer->vectorscope_float.uav[0], NULL);

    {
        uint32_t thread_groups[] = {8, 8, 1};
        uint32_t dispatch_x = (renderer->vectorscope_float.width + (thread_groups[0] - 1)) / thread_groups[0];
        uint32_t dispatch_y = (renderer->vectorscope_float.width + (thread_groups[1] - 1)) / thread_groups[1];
        uint32_t dispatch_z = thread_groups[2];

        context->lpVtbl->Dispatch(context, dispatch_x, dispatch_y, dispatch_z);
    }

    // Unbind UAV
    context->lpVtbl->CSSetUnorderedAccessViews(context, 0, 1, &nulluav, NULL);
}

void renderer_draw_ui(renderer_t *renderer, struct ui_state *ui_state, struct ui_element *root, bool debug_view) {
    ID3D11DeviceContext1 *context = renderer->context;
    float clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};

    // Binding states
    context->lpVtbl->RSSetState(context, renderer->rasterizer_states[debug_view ? RASTER_2D_WIREFRAME : RASTER_2D_DEFAULT]);
    context->lpVtbl->OMSetBlendState(context, renderer->blend_states[BLEND_ALPHA], NULL, 0xFFFFFFFF);

    // Clear and bind the render target (no depth needed)
    context->lpVtbl->ClearRenderTargetView(context, renderer->ui_rt.rtv[0], clear_color);
    context->lpVtbl->OMSetRenderTargets(context, 1, &renderer->ui_rt.rtv[0], NULL);

    // Bind the shader pipeline
    shader_pipeline_bind(context, &renderer->passes.ui);

    // Bind the sampler
    context->lpVtbl->PSSetSamplers(context, 0, 1, &renderer->sampler_states[SAMPLER_LINEAR_CLAMP]);

    // Set the viewport
    D3D11_VIEWPORT viewport = {
        .Width = (float)renderer->ui_rt.width,
        .Height = (float)renderer->ui_rt.height,
        .MinDepth = 0.0f,
        .MaxDepth = 1.0f,
    };
    context->lpVtbl->RSSetViewports(context, 1, &viewport);

    ui_draw(ui_state, renderer, root, debug_view);
}

void renderer_draw_composite(renderer_t *renderer) {
    ID3D11DeviceContext1 *context = renderer->context;
    float clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};

    // Output to Swapchain
    {
        // Binding states
        context->lpVtbl->RSSetState(context, renderer->rasterizer_states[RASTER_2D_DEFAULT]);
        context->lpVtbl->OMSetBlendState(context, renderer->blend_states[BLEND_OPAQUE], NULL, 0xFFFFFFFF);

        // Clear and bind the render target (no depth needed)
        context->lpVtbl->ClearRenderTargetView(context, renderer->swapchain.texture->rtv[0], clear_color);
        context->lpVtbl->OMSetRenderTargets(context, 1, &renderer->swapchain.texture->rtv[0], NULL);

        // Bind the shader pipeline
        shader_pipeline_bind(context, &renderer->passes.composite);

        // Bind the sampler
        context->lpVtbl->PSSetSamplers(context, 0, 1, &renderer->sampler_states[SAMPLER_LINEAR_CLAMP]);

        // Set the viewport
        D3D11_VIEWPORT viewport = {
            .Width = (float)renderer->swapchain.texture->width,
            .Height = (float)renderer->swapchain.texture->height,
            .MinDepth = 0.0f,
            .MaxDepth = 1.0f,
        };
        context->lpVtbl->RSSetViewports(context, 1, &viewport);

        // Bind the SRVs
        ID3D11ShaderResourceView *srvs[] = {
            renderer->vectorscope_float.srv,
            renderer->ui_rt.srv,
        };
        context->lpVtbl->PSSetShaderResources(context, 0, ARRAYSIZE(srvs), srvs);

        context->lpVtbl->Draw(context, 3, 0);

        // Unbind SRV
        ID3D11ShaderResourceView *nullsrv = NULL;
        context->lpVtbl->PSSetShaderResources(context, 0, 1, &nullsrv);
    }
}

void renderer_end_frame(renderer_t *renderer) {
    renderer->swapchain.swapchain->lpVtbl->Present(renderer->swapchain.swapchain, 1, 0);
}

void check_d3d11_debug_messages(ID3D11Device *device) {
#ifdef _DEBUG
    ID3D11InfoQueue *info_queue = NULL;
    HRESULT hr = device->lpVtbl->QueryInterface(device, &IID_ID3D11InfoQueue, (void **)&info_queue);

    if (SUCCEEDED(hr) && info_queue) {
        UINT64 message_count = info_queue->lpVtbl->GetNumStoredMessages(info_queue);

        for (UINT64 i = 0; i < message_count; i++) {
            SIZE_T message_length = 0;

            // Get message size first
            hr = info_queue->lpVtbl->GetMessage(info_queue, i, NULL, &message_length);
            if (SUCCEEDED(hr)) {
                // Allocate and get the actual message
                D3D11_MESSAGE *message = (D3D11_MESSAGE *)malloc(message_length);
                if (message) {
                    hr = info_queue->lpVtbl->GetMessage(info_queue, i, message, &message_length);
                    if (SUCCEEDED(hr)) {
                        const char *severity_str = "UNKNOWN";
                        switch (message->Severity) {
                            case D3D11_MESSAGE_SEVERITY_CORRUPTION:
                                severity_str = "CORRUPTION";
                                break;
                            case D3D11_MESSAGE_SEVERITY_ERROR:
                                severity_str = "ERROR";
                                break;
                            case D3D11_MESSAGE_SEVERITY_WARNING:
                                severity_str = "WARNING";
                                break;
                            case D3D11_MESSAGE_SEVERITY_INFO:
                                severity_str = "INFO";
                                break;
                            case D3D11_MESSAGE_SEVERITY_MESSAGE:
                                severity_str = "MESSAGE";
                                break;
                        }

                        // Or output to console/file
                        LOG("D3D11 %s: %s", severity_str, message->pDescription);
                    }
                    free(message);
                }
            }
        }

        // Clear processed messages
        info_queue->lpVtbl->ClearStoredMessages(info_queue);
        info_queue->lpVtbl->Release(info_queue);
    }
#endif
}

static bool create_device(ID3D11Device1 **device, ID3D11DeviceContext1 **context, D3D_FEATURE_LEVEL *feature_level) {
    UINT create_device_flags = 0;
#ifdef _DEBUG
    create_device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    // Hardware first, software as fallback
    D3D_DRIVER_TYPE driver_types[] = {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP};

    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0};

    ID3D11Device *base_device = NULL;
    ID3D11DeviceContext *base_context = NULL;
    D3D_FEATURE_LEVEL achieved_level = {0};

    HRESULT hr = E_FAIL;
    for (int i = 0; i < (int)ARRAYSIZE(driver_types); i++) {
        hr = D3D11CreateDevice(
            NULL,
            driver_types[i],
            NULL,
            create_device_flags,
            feature_levels,
            ARRAYSIZE(feature_levels),
            D3D11_SDK_VERSION,
            &base_device,
            &achieved_level,
            &base_context);

        if (SUCCEEDED(hr)) {
            LOG("D3D11 base device created successfully. Feature level: 0x%x, Driver Type: %s",
                achieved_level, (driver_types[i] == D3D_DRIVER_TYPE_HARDWARE) ? "Hardware" : "WARP");
            break;
        }
    }

    if (FAILED(hr)) {
        LOG("Failed to create D3D11 device with any driver type");
        return false;
    }

    // Upgrade to D3D11.1 - Device
    hr = base_device->lpVtbl->QueryInterface(base_device, IID_PPV_ARGS_C(ID3D11Device1, device));
    if (FAILED(hr)) {
        LOG("Failed to upgrade to ID3D11Device1");
        base_device->lpVtbl->Release(base_device);
        base_context->lpVtbl->Release(base_context);
        return false;
    }

    // Upgrade Device Context, as well.
    hr = base_context->lpVtbl->QueryInterface(base_context, IID_PPV_ARGS_C(ID3D11DeviceContext1, context));
    if (FAILED(hr)) {
        LOG("Failed to upgrade to ID3D11DeviceContext1");
        (*device)->lpVtbl->Release(*device);
        base_device->lpVtbl->Release(base_device);
        base_context->lpVtbl->Release(base_context);
        return false;
    }

    // Save achieved feature level in case it's needed
    *feature_level = achieved_level;

#ifdef _DEBUG
    // Set up enhanced debug layer
    ID3D11InfoQueue *info_queue = NULL;
    hr = (*device)->lpVtbl->QueryInterface(*device, IID_PPV_ARGS_C(ID3D11InfoQueue, &info_queue));

    if (SUCCEEDED(hr) && info_queue) {
        // No breaking on errors, only logging
        info_queue->lpVtbl->SetBreakOnSeverity(info_queue, D3D11_MESSAGE_SEVERITY_CORRUPTION, FALSE);
        info_queue->lpVtbl->SetBreakOnSeverity(info_queue, D3D11_MESSAGE_SEVERITY_ERROR, FALSE);
        info_queue->lpVtbl->SetBreakOnSeverity(info_queue, D3D11_MESSAGE_SEVERITY_WARNING, FALSE);

        // Enable message storage so we can retrieve them
        info_queue->lpVtbl->SetMuteDebugOutput(info_queue, FALSE);

        // Set storage limit
        info_queue->lpVtbl->SetMessageCountLimit(info_queue, 1024);

        LOG("D3D11 debug layer enabled for logging");
        info_queue->lpVtbl->Release(info_queue);
    } else {
        LOG("Failed to enable D3D11 debug layer");
    }
#endif

    // Release remaining not needed resources
    base_device->lpVtbl->Release(base_device);
    base_context->lpVtbl->Release(base_context);

    return true;
}

static bool create_swapchain(ID3D11Device1 *device, HWND hwnd, texture_t *swapchain_texture, IDXGISwapChain3 **swapchain) {
    // Get the DXGI device
    IDXGIDevice *dxgi_device = NULL;
    HRESULT hr = device->lpVtbl->QueryInterface(device, IID_PPV_ARGS_C(IDXGIDevice, &dxgi_device));
    if (FAILED(hr)) {
        LOG("Failed to get DXGI Device");
        return false;
    }

    // Get the adapter from the DXGI Device
    IDXGIAdapter *adapter = NULL;
    hr = dxgi_device->lpVtbl->GetAdapter(dxgi_device, &adapter);
    if (FAILED(hr)) {
        LOG("Failed to get DXGI Adapter");
        dxgi_device->lpVtbl->Release(dxgi_device);
        return false;
    }

    // Get the factory from the adapter
    IDXGIFactory2 *factory2 = NULL;
    hr = adapter->lpVtbl->GetParent(adapter, IID_PPV_ARGS_C(IDXGIFactory2, &factory2));
    if (FAILED(hr)) {
        LOG("Failed to get DXGI Factory");
        adapter->lpVtbl->Release(adapter);
        dxgi_device->lpVtbl->Release(dxgi_device);
        return false;
    }

    // Swapchain description
    DXGI_SWAP_CHAIN_DESC1 desc = {
        .BufferCount = 2,
        .Width = 0,  // Zero here means it defaults to window width
        .Height = 0, // ... and to window height.
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .SampleDesc = {
            .Count = 1,
            .Quality = 0},
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .Scaling = DXGI_SCALING_STRETCH,
        .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
    };

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen_desc = {
        .Windowed = TRUE,
    };

    // Create the swapchain
    IDXGISwapChain1 *swapchain1 = NULL;
    hr = factory2->lpVtbl->CreateSwapChainForHwnd(factory2, (IUnknown *)device, hwnd, &desc, &fullscreen_desc, NULL, &swapchain1);
    if (FAILED(hr)) {
        LOG("Failed to create base swapchain");
        factory2->lpVtbl->Release(factory2);
        adapter->lpVtbl->Release(adapter);
        dxgi_device->lpVtbl->Release(dxgi_device);
        return false;
    }

    // Upgrade to Swapchain3
    hr = swapchain1->lpVtbl->QueryInterface(swapchain1, IID_PPV_ARGS_C(IDXGISwapChain3, swapchain));
    if (FAILED(hr)) {
        LOG("Failed to upgrade base swapchain to Swapchain3");
        swapchain1->lpVtbl->Release(swapchain1);
        adapter->lpVtbl->Release(adapter);
        dxgi_device->lpVtbl->Release(dxgi_device);
        return false;
    }

    // Release remaining resources
    swapchain1->lpVtbl->Release(swapchain1);
    adapter->lpVtbl->Release(adapter);
    dxgi_device->lpVtbl->Release(dxgi_device);

    // I could create the RTV for the swapchain here.
    if (!texture_create_from_backbuffer(device, *swapchain, swapchain_texture)) {
        LOG("Failed to create swapchain texture from backbuffer");
        return false;
    }

    return true;
}

static void destroy_swapchain(swapchain_t *swapchain) {
    if (swapchain) {
        // Free the swapchain's texture
        if (swapchain->texture) {
            texture_destroy(swapchain->texture);
            free(swapchain->texture);
            swapchain->texture = NULL;
        }

        if (swapchain->swapchain) {
            swapchain->swapchain->lpVtbl->Release(swapchain->swapchain);
            swapchain->swapchain = NULL;
        }
    }
}

static bool create_pipeline_states(renderer_t *renderer) {
    ID3D11Device1 *device = renderer->device;

    // --- Rasterizer States ---
    {
        D3D11_RASTERIZER_DESC base = {
            .FillMode = D3D11_FILL_SOLID,
            .CullMode = D3D11_CULL_NONE,
            .FrontCounterClockwise = FALSE,
            .DepthBias = D3D11_DEFAULT_DEPTH_BIAS,
            .DepthBiasClamp = D3D11_DEFAULT_DEPTH_BIAS_CLAMP,
            .SlopeScaledDepthBias = D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
            .DepthClipEnable = FALSE,
            .ScissorEnable = FALSE,
            .MultisampleEnable = FALSE,
            .AntialiasedLineEnable = FALSE,
        };

        // Default -- no culling, solid fill
        {
            D3D11_RASTERIZER_DESC desc = base;
            device->lpVtbl->CreateRasterizerState(device, &desc, &renderer->rasterizer_states[RASTER_2D_DEFAULT]);
        }

        // Scissor-enabled state for UI clipping
        {
            D3D11_RASTERIZER_DESC desc = base;
            desc.ScissorEnable = TRUE;
            device->lpVtbl->CreateRasterizerState(device, &desc, &renderer->rasterizer_states[RASTER_2D_SCISSOR]);
        }

        // Wireframe state for debugging
        {
            D3D11_RASTERIZER_DESC desc = base;
            desc.FillMode = D3D11_FILL_WIREFRAME;
            device->lpVtbl->CreateRasterizerState(device, &desc, &renderer->rasterizer_states[RASTER_2D_WIREFRAME]);
        }
    }

    // --- Blend States ---
    {
        D3D11_BLEND_DESC base = {
            .AlphaToCoverageEnable = FALSE,
            .IndependentBlendEnable = FALSE,
        };
        for (int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
            base.RenderTarget[i].BlendEnable = FALSE;
            base.RenderTarget[i].SrcBlend = D3D11_BLEND_ONE;
            base.RenderTarget[i].DestBlend = D3D11_BLEND_ZERO;
            base.RenderTarget[i].BlendOp = D3D11_BLEND_OP_ADD;
            base.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_ONE;
            base.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_ZERO;
            base.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_ADD;
            base.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        }

        // Opaque - no blending
        {
            D3D11_BLEND_DESC desc = base;
            device->lpVtbl->CreateBlendState(device, &desc, &renderer->blend_states[BLEND_OPAQUE]);
        }

        // Alpha blending
        {
            D3D11_BLEND_DESC desc = base;
            desc.RenderTarget[0].BlendEnable = TRUE;
            desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
            desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
            desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
            desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
            desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
            desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
            device->lpVtbl->CreateBlendState(device, &desc, &renderer->blend_states[BLEND_ALPHA]);
        }

        // Additive blending
        {
            D3D11_BLEND_DESC desc = base;
            desc.RenderTarget[0].BlendEnable = TRUE;
            desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
            desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
            desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
            desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
            device->lpVtbl->CreateBlendState(device, &desc, &renderer->blend_states[BLEND_ADDITIVE]);
        }

        // Multiply blending
        {
            D3D11_BLEND_DESC desc = base;
            desc.RenderTarget[0].BlendEnable = TRUE;
            desc.RenderTarget[0].SrcBlend = D3D11_BLEND_DEST_COLOR;
            desc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
            desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_DEST_ALPHA;
            desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
            device->lpVtbl->CreateBlendState(device, &desc, &renderer->blend_states[BLEND_MULTIPLY]);
        }

        // Premultiplied alpha
        {
            D3D11_BLEND_DESC desc = base;
            desc.RenderTarget[0].BlendEnable = TRUE;
            desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
            desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
            desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
            desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
            device->lpVtbl->CreateBlendState(device, &desc, &renderer->blend_states[BLEND_PREMULT_ALPHA]);
        }
    }

    // --- Sampler States ---
    {
        D3D11_SAMPLER_DESC base = {
            .Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
            .AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
            .AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
            .AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
            .MipLODBias = 0,
            .MaxAnisotropy = 1,
            .ComparisonFunc = D3D11_COMPARISON_NEVER,
            .BorderColor = {1.0f, 1.0f, 1.0f, 1.0f},
            .MinLOD = -3.402823466e+38f,
            .MaxLOD = 3.402823466e+38f,
        };

        {
            D3D11_SAMPLER_DESC desc = base;
            desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
            desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
            desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
            device->lpVtbl->CreateSamplerState(device, &desc, &renderer->sampler_states[SAMPLER_LINEAR_WRAP]);
        }

        {
            D3D11_SAMPLER_DESC desc = base;
            desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
            desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
            desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            device->lpVtbl->CreateSamplerState(device, &desc, &renderer->sampler_states[SAMPLER_LINEAR_CLAMP]);
        }

        {
            D3D11_SAMPLER_DESC desc = base;
            desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
            desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
            desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
            desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
            device->lpVtbl->CreateSamplerState(device, &desc, &renderer->sampler_states[SAMPLER_POINT_WRAP]);
        }

        {
            D3D11_SAMPLER_DESC desc = base;
            desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
            desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
            desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
            desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            device->lpVtbl->CreateSamplerState(device, &desc, &renderer->sampler_states[SAMPLER_POINT_CLAMP]);
        }

        {
            D3D11_SAMPLER_DESC desc = base;
            desc.Filter = D3D11_FILTER_ANISOTROPIC;
            desc.MaxAnisotropy = 16;
            desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
            desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
            desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            device->lpVtbl->CreateSamplerState(device, &desc, &renderer->sampler_states[SAMPLER_ANISOTROPIC_CLAMP]);
        }
    }

    return true;
}

static bool create_textures(renderer_t *renderer) {
    ID3D11Device1 *device = renderer->device;

    // Create texture for capture blitting
    {
        const texture_desc_t desc = {
            .width = 500,
            .height = 500,
            .format = DXGI_FORMAT_B8G8R8A8_UNORM,
            .array_size = 1,
            .bind_flags = D3D11_BIND_SHADER_RESOURCE,
            .mip_levels = 1,
            .msaa_samples = 1,
            .generate_srv = true,
        };

        if (!texture_create(device, &desc, &renderer->blit_texture)) {
            LOG("Failed to create texture for capture blitting");
            return false;
        }
        LOG("Capture blit texture created");
    }

    // Create texture for vectorscope
    {
        const texture_desc_t desc = {
            .width = 1024,
            .height = 1024,
            .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .array_size = 1,
            .bind_flags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
            .mip_levels = 1,
            .msaa_samples = 1,
            .generate_srv = true,
        };

        if (!texture_create(device, &desc, &renderer->vectorscope_texture)) {
            LOG("Failed to create texture for vectorscope");
            return false;
        }

        const texture_desc_t desc1 = {
            .width = 1024,
            .height = 1024,
            .format = DXGI_FORMAT_R32_UINT,
            .array_size = 1,
            .bind_flags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
            .mip_levels = 1,
            .msaa_samples = 1,
            .generate_srv = true,
        };

        if (!texture_create(device, &desc1, &renderer->vectorscope_buckets)) {
            LOG("Failed to create texture for vectorscope");
            return false;
        }

        const texture_desc_t desc2 = {
            .width = 1024,
            .height = 1024,
            .format = DXGI_FORMAT_R32_FLOAT,
            .array_size = 1,
            .bind_flags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
            .mip_levels = 1,
            .msaa_samples = 1,
            .generate_srv = true,
        };

        if (!texture_create(device, &desc2, &renderer->vectorscope_float)) {
            LOG("Failed to create texture for vectorscope");
            return false;
        }

        const texture_desc_t out_desc = {
            .width = 1024,
            .height = 576,
            .format = DXGI_FORMAT_R8G8B8A8_UNORM,
            .array_size = 1,
            .bind_flags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
            .mip_levels = 1,
            .msaa_samples = 1,
            .generate_srv = true,
        };

        if (!texture_create(device, &out_desc, &renderer->vectorscope_out)) {
            LOG("Failed to create texture for vectorscope");
            return false;
        }

        LOG("Vectorscope texture created");
    }

    // Create texture for UI pass
    {
        const texture_desc_t desc = {
            .width = renderer->window->width,
            .height = renderer->window->height,
            .format = DXGI_FORMAT_R8G8B8A8_UNORM,
            .array_size = 1,
            .bind_flags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
            .mip_levels = 1,
            .msaa_samples = 1,
            .generate_srv = true,
        };

        if (!texture_create(device, &desc, &renderer->ui_rt)) {
            LOG("Failed to create texture for UI render target");
            return false;
        }
        LOG("UI render target texture created");
    }

    // Create 1px white texture
    // TODO: I am changing this to transparent 1px texture
    // have to update name and comments and usage accordingly
    {
        uint8_t data[4] = {0, 0, 0, 0};

        if (!texture_create_from_data(device, data, 1, 1, &renderer->default_white_px)) {
            LOG("Failed to create default 1px white texture");
            return false;
        }
        LOG("1px white texture created");
    }

    return true;
}

static bool create_shader_pipelines(renderer_t *renderer) {
    ID3D11Device1 *device = renderer->device;

    // Create full-screen triangle vertex shader
    {
        if (!shader_create_from_file(
                device,
                "assets/shaders/fullscreen_triangle.vs.hlsl",
                SHADER_STAGE_VS,
                "main",
                &renderer->shaders.fs_triangle_vs)) {
            LOG("Failed to create full-screen triangle vertex shader");
            return false;
        }
    }

    // Create shader pipelines for vectorscope
    {
        if (!shader_create_from_file(
                device,
                "assets/shaders/vs_accum.cs.hlsl",
                SHADER_STAGE_CS,
                "main",
                &renderer->shaders.vs_accum_cs)) {
            LOG("Failed to create compute shader for Vectorscope Accumulation Pass");
            return false;
        }

        shader_t *shaders[] = {&renderer->shaders.vs_accum_cs};
        if (!shader_pipeline_create(
                device,
                shaders,
                ARRAYSIZE(shaders),
                NULL,
                0,
                &renderer->passes.vs_accum)) {
            LOG("Failed to create shader pipeline for Vectorscope Accumulation Pass");
            return false;
        }

        if (!shader_create_from_file(
                device,
                "assets/shaders/vs_blur.cs.hlsl",
                SHADER_STAGE_CS,
                "main",
                &renderer->shaders.vs_blur_cs)) {
            LOG("Failed to create compute shader for Vectorscope Blur Pass");
            return false;
        }

        shader_t *shaders1[] = {&renderer->shaders.vs_blur_cs};
        if (!shader_pipeline_create(
                device,
                shaders1,
                ARRAYSIZE(shaders1),
                NULL,
                0,
                &renderer->passes.vs_blur)) {
            LOG("Failed to create shader pipeline for Vectorscope Blur Pass");
            return false;
        }

        if (!shader_create_from_file(
                device,
                "assets/shaders/vs_comp.cs.hlsl",
                SHADER_STAGE_CS,
                "main",
                &renderer->shaders.vs_comp_cs)) {
            LOG("Failed to create compute shader for Vectorscope Composite Pass");
            return false;
        }

        shader_t *shaders2[] = {&renderer->shaders.vs_comp_cs};
        if (!shader_pipeline_create(
                device,
                shaders2,
                ARRAYSIZE(shaders2),
                NULL,
                0,
                &renderer->passes.vs_comp)) {
            LOG("Failed to create shader pipeline for Vectorscope Composite Pass");
            return false;
        }
    }

    // Create shader pipelines for waveform and parade
    {
        if (!shader_create_from_file(
                device,
                "assets/shaders/wf_accum.cs.hlsl",
                SHADER_STAGE_CS,
                "main",
                &renderer->shaders.wf_accum_cs)) {
            LOG("Failed to create compute shader for Waveform Accumulation Pass");
            return false;
        }

        shader_t *shaders[] = {&renderer->shaders.wf_accum_cs};
        if (!shader_pipeline_create(
                device,
                shaders,
                ARRAYSIZE(shaders),
                NULL,
                0,
                &renderer->passes.wf_accum)) {
            LOG("Failed to create shader pipeline for Waveform Accumulation Pass");
            return false;
        }

        if (!shader_create_from_file(
                device,
                "assets/shaders/wf_comp.cs.hlsl",
                SHADER_STAGE_CS,
                "main",
                &renderer->shaders.wf_comp_cs)) {
            LOG("Failed to create compute shader for Waveform Composite Pass");
            return false;
        }

        shader_t *shaders2[] = {&renderer->shaders.wf_comp_cs};
        if (!shader_pipeline_create(
                device,
                shaders2,
                ARRAYSIZE(shaders2),
                NULL,
                0,
                &renderer->passes.wf_comp)) {
            LOG("Failed to create shader pipeline for Waveform Composite Pass");
            return false;
        }
    }

    // Create shader pipeline for composite pass
    {
        if (!shader_create_from_file(
                device,
                "assets/shaders/comp.ps.hlsl",
                SHADER_STAGE_PS,
                "main",
                &renderer->shaders.composite_ps)) {
            LOG("Failed to create pixel shader for Composite Pass");
            return false;
        }

        shader_t *shaders[] = {&renderer->shaders.fs_triangle_vs, &renderer->shaders.composite_ps};

        if (!shader_pipeline_create(
                device,
                shaders,
                ARRAYSIZE(shaders),
                NULL,
                0,
                &renderer->passes.composite)) {
            LOG("Failed to create shader pipeline for Composite Pass");
            return false;
        }
    }

    // Create shader pipeline for UI pass
    {
        if (!shader_create_from_file(
                device,
                "assets/shaders/unit_quad.vs.hlsl",
                SHADER_STAGE_VS,
                "main",
                &renderer->shaders.unit_quad_vs)) {
            LOG("Failed to create unit quad vertex shader");
            return false;
        }

        if (!shader_create_from_file(
                device,
                "assets/shaders/ui.ps.hlsl",
                SHADER_STAGE_PS,
                "main",
                &renderer->shaders.ui_ps)) {
            LOG("Failed to create pixel shader for UI Pass");
            return false;
        }

        shader_t *shaders[] = {&renderer->shaders.unit_quad_vs, &renderer->shaders.ui_ps};

        if (!shader_pipeline_create(
                device,
                shaders,
                ARRAYSIZE(shaders),
                NULL,
                0,
                &renderer->passes.ui)) {
            LOG("Failed to create shader pipeline for UI Pass");
            return false;
        }
    }

    return true;
}

static bool create_constant_buffers(renderer_t *renderer) {
    ID3D11Device1 *device = renderer->device;

    // Create Constant Buffer for Per Frame Data
    {
        D3D11_BUFFER_DESC desc = {
            .Usage = D3D11_USAGE_DYNAMIC,
            .ByteWidth = sizeof(struct per_frame_data),
            .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
            .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
        };

        HRESULT hr = device->lpVtbl->CreateBuffer(device, &desc, NULL, &renderer->per_frame_buffer);
        if (FAILED(hr)) {
            LOG("Failed to create constant buffer for Per Frame Data");
            return false;
        }
    }

    // Create Constant Buffer for Per UI Mesh Data
    {
        D3D11_BUFFER_DESC desc = {
            .Usage = D3D11_USAGE_DYNAMIC,
            .ByteWidth = sizeof(struct per_ui_mesh_data),
            .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
            .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
        };

        HRESULT hr = device->lpVtbl->CreateBuffer(device, &desc, NULL, &renderer->per_ui_mesh_buffer);
        if (FAILED(hr)) {
            LOG("Failed to create constant buffer for Per UI Mesh Data");
            return false;
        }
    }

    return true;
}
