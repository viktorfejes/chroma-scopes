#include "shader.h"

#include "logger.h"
#include "macros.h"

#include <assert.h>
#include <string.h>

#include <d3dcompiler.h>
#include <stringapiset.h>
#include <winerror.h>

bool shader_create_from_file(ID3D11Device1 *device, const char *path, shader_stage_t stage, const char *entry_point, shader_t *out_shader) {
    assert(out_shader && "Out shader cannot be NULL");
    assert(device && "ID3D11Device cannot be NULL");

    UINT compile_flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    compile_flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = E_FAIL;
    ID3DBlob *error_blob = NULL;
    ID3DBlob *shader_blob = NULL;

    // Targets for different shader stages
    static const char *shader_target[SHADER_STAGE_COUNT] = {"vs_5_0", "ps_5_0", "cs_5_0"};

    // Convert char to wchar
    int len = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    wchar_t *path_wide = malloc(len * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, path, -1, path_wide, len);

    // Compile the file from file using d3dcompiler
    hr = D3DCompileFromFile(
        path_wide,
        NULL,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entry_point,
        shader_target[stage],
        compile_flags,
        0,
        &shader_blob,
        &error_blob);

    // Assuming no matter what happens I can free the path_wide here
    free(path_wide);

    if (FAILED(hr)) {
        if (error_blob) {
            LOG("%s: Shader module failed to compile from file: %ls. Error: %s", __func__, path, (char *)error_blob->lpVtbl->GetBufferPointer(error_blob));
            error_blob->lpVtbl->Release(error_blob);
        }
        return false;
    }

    // Get bytecode data
    const void *bytecode = shader_blob->lpVtbl->GetBufferPointer(shader_blob);
    size_t bytecode_size = shader_blob->lpVtbl->GetBufferSize(shader_blob);

    // Pass it to the function that can take it home
    bool success = shader_create_from_bytecode(device, stage, bytecode, bytecode_size, out_shader);

    // Cleanup
    if (error_blob) error_blob->lpVtbl->Release(error_blob);
    if (shader_blob) shader_blob->lpVtbl->Release(shader_blob);

    return success;
}

bool shader_create_from_bytecode(ID3D11Device1 *device, shader_stage_t stage, const void *bytecode, size_t bytecode_size, shader_t *out_shader) {
    HRESULT hr = E_FAIL;

    switch (stage) {
        case SHADER_STAGE_VS: {
            hr = device->lpVtbl->CreateVertexShader(
                device,
                bytecode,
                bytecode_size,
                NULL,
                &out_shader->shader.vs);

            // Copy the bytecode -- we'll need it for input layout
            if (SUCCEEDED(hr)) {
                out_shader->vs_bytecode = malloc(bytecode_size);
                if (!out_shader->vs_bytecode) {
                    LOG("Failed to allocate memory for vertex shader bytecode");
                    out_shader->shader.vs->lpVtbl->Release(out_shader->shader.vs);
                    hr = E_OUTOFMEMORY;
                    break;
                }
                memcpy(out_shader->vs_bytecode, bytecode, bytecode_size);
                out_shader->vs_bytecode_size = bytecode_size;
            }
        } break;

        case SHADER_STAGE_PS: {
            hr = device->lpVtbl->CreatePixelShader(
                device,
                bytecode,
                bytecode_size,
                NULL,
                &out_shader->shader.ps);
        } break;

        case SHADER_STAGE_CS: {
            hr = device->lpVtbl->CreateComputeShader(
                device,
                bytecode,
                bytecode_size,
                NULL,
                &out_shader->shader.cs);
        } break;

        default: {
            LOG("%s: Unknown shader stage", __func__);
            hr = E_FAIL;
        } break;
    }

    // Check if the shader was successfully created or not
    if (FAILED(hr)) {
        LOG("%s: Shader creation failed", __func__);
        // Cleanup allocated bytecode
        if (out_shader->vs_bytecode) {
            free(out_shader->vs_bytecode);
            memset(out_shader, 0, sizeof(shader_t));
        }
        return false;
    }

    out_shader->stage = stage;
    return true;
}

void shader_destroy(shader_t *shader) {
    if (shader) {
        switch (shader->stage) {
            case SHADER_STAGE_VS:
                if (shader->shader.vs) {
                    shader->shader.vs->lpVtbl->Release(shader->shader.vs);
                }
                if (shader->vs_bytecode) {
                    free(shader->vs_bytecode);
                }
                break;
            case SHADER_STAGE_PS:
                if (shader->shader.ps) {
                    shader->shader.ps->lpVtbl->Release(shader->shader.ps);
                }
                break;
            case SHADER_STAGE_CS:
                if (shader->shader.cs) {
                    shader->shader.cs->lpVtbl->Release(shader->shader.cs);
                }
                break;
            default:
                break;
        }

        memset(shader, 0, sizeof(shader_t));
    }
}

bool shader_bind(shader_t *shader) {
    UNUSED(shader);
    return true;
}

bool shader_pipeline_create(ID3D11Device1 *device, shader_t **shaders, uint8_t shader_count, const D3D11_INPUT_ELEMENT_DESC *input_desc, uint16_t input_count, shader_pipeline_t *out_pipeline) {
    assert(device && "Device cannot be NULL");
    assert(shaders && "Shaders array cannot be NULL");
    assert(out_pipeline && "Output shader pipeline cannot be NULL");
    assert(shader_count > 0 && "Must have at least one shader");

    // Zero out the shader pipeline struct just in case we have some garbage in there
    memset(out_pipeline, 0, sizeof(shader_pipeline_t));

    void *vs_bytecode = NULL;
    size_t vs_bytecode_size = 0;

    for (uint8_t i = 0; i < shader_count; ++i) {
        shader_t *s = shaders[i];

        // Validate shader stage
        if (s->stage >= SHADER_STAGE_COUNT) {
            LOG("Invalid shader stage %d", s->stage);
            return false;
        }

        // Check for duplicate stages
        if (out_pipeline->stage[s->stage] != NULL) {
            LOG("Duplicate shader stage %d", s->stage);
            return false;
        }

        // Store the pointer to the shader in the pipeline
        out_pipeline->stage[s->stage] = s;

        // Check if the module is a vertex module
        if (s->stage == SHADER_STAGE_VS) {
            vs_bytecode = s->vs_bytecode;
            vs_bytecode_size = s->vs_bytecode_size;
        }
    }

    if (vs_bytecode && input_desc && input_count > 0) {
        HRESULT hr = device->lpVtbl->CreateInputLayout(
            device,
            input_desc,
            input_count,
            vs_bytecode,
            vs_bytecode_size,
            &out_pipeline->input_layout);

        if (FAILED(hr)) {
            LOG("%s: Couldn't create an input layout for the vertex shader in the pipeline", __func__);
            return false;
        }
    }

    return true;
}

bool shader_pipeline_bind(ID3D11DeviceContext1 *context, shader_pipeline_t *pipeline) {
    // Vertex Shader
    context->lpVtbl->VSSetShader(context, 
        pipeline->stage[SHADER_STAGE_VS] ? pipeline->stage[SHADER_STAGE_VS]->shader.vs : NULL, 
        NULL, 0);
    
    // Pixel Shader
    context->lpVtbl->PSSetShader(context, 
        pipeline->stage[SHADER_STAGE_PS] ? pipeline->stage[SHADER_STAGE_PS]->shader.ps : NULL, 
        NULL, 0);
    
    // Compute Shader
    context->lpVtbl->CSSetShader(context, 
        pipeline->stage[SHADER_STAGE_CS] ? pipeline->stage[SHADER_STAGE_CS]->shader.cs : NULL, 
        NULL, 0);

    // Input Layout
    context->lpVtbl->IASetInputLayout(context, pipeline->input_layout);

    return true;
}

void shader_pipeline_destroy(shader_pipeline_t *pipeline) {
    if (pipeline) {
        // Release input layout if exists
        if (pipeline->input_layout) {
            pipeline->input_layout->lpVtbl->Release(pipeline->input_layout);
        }

        // NOTE: We don't release the shaders here since the pipeline doesn't own them.
        // Shaders should be cleaned up separately.

        memset(pipeline, 0, sizeof(shader_pipeline_t));
    }
}
