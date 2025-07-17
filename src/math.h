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

// Column-major order
typedef struct float4x4 {
    float m[16];
} float4x4_t;

typedef struct rect {
    float x, y;
    float width, height;
} rect_t;

#define M(mat, row, col) ((mat).m[(col) * 4 + (row)])

/* @brief Automatically centered, optimized orthographic matrix projection. Left-handed */
float4x4_t mat_orthographic_lh(float view_width, float view_height, float near_z, float far_z);
/* @brief Generic offcenter, optimized orthographic matrix projection. Left-handed*/
float4x4_t mat_orthographic_offcenter_lh(float left, float right, float bottom, float top, float near_z, float far_z);

float2_t rect_to_position(rect_t rect);
float2_t rect_to_size(rect_t rect);
