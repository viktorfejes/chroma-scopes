#pragma once

typedef struct shader {
    shader_stage_t stage;
    union {
        ID3D11VertexShader *vs;
        ID3D11PixelShader *ps;
        ID3D11ComputeShader *cs;
    };
} shader_t;

typedef struct shader_program {
    shader_t *shaders[SHADER_STAGE_COUNT];

    ID3D11SamplerState **samplers;
    uint8_t sampler_count;
} shader_program_t;
