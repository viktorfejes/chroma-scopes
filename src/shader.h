#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <d3d11_1.h>

typedef enum shader_stage {
    SHADER_STAGE_VS,
    SHADER_STAGE_PS,
    SHADER_STAGE_CS,
    SHADER_STAGE_COUNT
} shader_stage_t;

typedef struct shader {
    shader_stage_t stage;

    union {
        ID3D11VertexShader *vs;
        ID3D11PixelShader *ps;
        ID3D11ComputeShader *cs;
    } shader;

    void *vs_bytecode;
    size_t vs_bytecode_size;
} shader_t;

typedef struct shader_pipeline {
    shader_t *stage[SHADER_STAGE_COUNT];
    ID3D11InputLayout *input_layout;
} shader_pipeline_t;

bool shader_create_from_file(ID3D11Device1 *device, const char *path, shader_stage_t stage, const char *entry_point, shader_t *out_shader);
bool shader_create_from_bytecode(ID3D11Device1 *device, shader_stage_t stage, const void *bytecode, size_t bytecode_size, shader_t *out_shader);
void shader_destroy(shader_t *shader);
bool shader_bind(shader_t *shader);

bool shader_pipeline_create(ID3D11Device1 *device, shader_t *shaders, uint8_t shader_count, const D3D11_INPUT_ELEMENT_DESC *input_desc, uint16_t input_count, shader_pipeline_t *out_pipeline);
bool shader_pipeline_bind(ID3D11DeviceContext1 *context, shader_pipeline_t *pipeline);
void shader_pipeline_destroy(shader_pipeline_t *pipeline);
