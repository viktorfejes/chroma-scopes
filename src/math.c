#include "math.h"

float4x4_t mat_orthographic_lh(float view_width, float view_height, float near_z, float far_z) {
    float z_range = 1.0f / (far_z - near_z);

    float4x4_t m = {0};
    M(m, 0, 0) = 2.0f / view_width;
    M(m, 1, 1) = 2.0f / view_height;
    M(m, 2, 2) = z_range;
    M(m, 2, 3) = -z_range * near_z;
    M(m, 3, 3) = 1.0f;

    return m;
}

float4x4_t mat_orthographic_offcenter_lh(float left, float right, float bottom, float top, float near_z, float far_z) {
    float inv_width = 1.0f / (right - left);
    float inv_height = 1.0f / (top - bottom);
    float z_range = 1.0f / (far_z - near_z);

    float4x4_t m = {0};
    M(m, 0, 0) = inv_width + inv_width;
    M(m, 0, 1) = 0.0f;
    M(m, 0, 2) = 0.0f;
    M(m, 0, 3) = -(left + right) * inv_width;

    M(m, 1, 0) = 0.0f;
    M(m, 1, 1) = inv_height + inv_height;
    M(m, 1, 2) = 0.0f;
    M(m, 1, 3) = -(top + bottom) * inv_height;

    M(m, 2, 0) = 0.0f;
    M(m, 2, 1) = 0.0f;
    M(m, 2, 2) = z_range;
    M(m, 2, 3) = -z_range * near_z;
    
    M(m, 3, 0) = 0.0f;
    M(m, 3, 1) = 0.0f;
    M(m, 3, 2) = 0.0f;
    M(m, 3, 3) = 1.0f;

    return m;
}

bool rect_contains(rect_t rect, float2_t point) {
    return point.x >= rect.x &&
           point.x < rect.x + rect.width &&
           point.y >= rect.y &&
           point.y < rect.y + rect.height;
}

float2_t rect_to_position(rect_t rect) {
    return (float2_t){
        .x = rect.x + (rect.width * 0.5f),
        .y = rect.y + (rect.height * 0.5f),
    };
}

float2_t rect_to_size(rect_t rect) {
    return (float2_t){
        .x = rect.width,
        .y = rect.height,
    };
}
