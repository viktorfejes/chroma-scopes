#include "math.h"

#include "macros.h"

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

int32_t rect_intersection_area(rect_t a, rect_t b) {
    int32_t x0 = MAX(a.x, b.x);
    int32_t y0 = MAX(a.y, b.y);
    int32_t x1 = MIN(a.x + a.width, b.x + b.width);
    int32_t y1 = MIN(a.y + a.height, b.y + b.height);

    int32_t w = x1 - x0;
    int32_t h = y1 - y0;

    // No intersection
    if (w <= 0 || h <= 0) return 0;

    return w * h;
}

rect_t rect_normalize(rect_t rect) {
    return (rect_t){
        .x = MIN(rect.x, rect.x + rect.width),
        .y = MIN(rect.y, rect.y + rect.height),
        .width = ABS(rect.width),
        .height = ABS(rect.height),
    };
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
