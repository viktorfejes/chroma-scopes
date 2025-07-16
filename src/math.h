#pragma once

typedef struct float2 {
    float x, y;
} float2_t;

typedef struct float3 {
    float x, y, z;
} float3_t;

typedef struct float4 {
    float x, y, z, w;
} float4_t;

// Column-major order for D3D11
typedef struct mat4 {
    float m[16];
} mat4_t;

mat4_t mat4_ortho(float left, float right, float top, float bottom, float near_z, float far_z);
