Texture2D<float> vs_blur_tex : register(t0);
RWTexture2D<float4> out_tex : register(u0);
SamplerState samp : register(s0);

cbuffer VSParams : register(b0) {
    float2 resolution;
    float2 padding;
};

// Precompute skintone line direction
static const float skintone_angle = radians(123.0);
static const float2 skintone_dir = float2(cos(skintone_angle), sin(skintone_angle));

// Vectors for RGB->YCbCr conversion
static const float3 RGB_to_Cb = float3(-0.1146, -0.3854, 0.5);
static const float3 RGB_to_Cr = float3(0.5, -0.4542, -0.0458);

// Main colors -- used mainly for the boxes and their calculations
static const float3 main_colors[6] = {
    float3(1.0, 0.0, 0.0), // Red
    float3(0.0, 1.0, 0.0), // Green
    float3(0.0, 0.0, 1.0), // Blue
    float3(1.0, 1.0, 0.0), // Yellow
    float3(0.0, 1.0, 1.0), // Cyan
    float3(1.0, 0.0, 1.0)  // Magenta
};

static const float line_thickness = 0.004;
static const float box_size = 0.1;
static const float3 overlay_color = float3(0.71f, 0.57f, 0.16f) * 0.6;
static const float scope_scale = 0.6;

float line_sdf(float2 p, float2 a, float2 b, float thickness) {
    float2 pa = p - a;
    float2 ba = b - a;
    float h = saturate(dot(pa, ba) / dot(ba, ba));
    return length(pa - ba * h) - thickness * 0.5;
}

float circle_sdf(float2 p, float2 center, float radius) {
    return length(p - center) - radius;
}

float box_sdf(float2 p, float2 center, float2 size) {
    float2 d = abs(p - center) - size * 0.5;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

float3 ycbcr_to_rgb(float3 ycbcr) {
    float Y = ycbcr.x;
    float Cb = ycbcr.y;
    float Cr = ycbcr.z;

    float r = Y + 1.402 * Cr;
    float g = Y - 0.344136 * Cb - 0.714136 * Cr;
    float b = Y + 1.772 * Cb;

    return saturate(float3(r, g, b));
}

[numthreads(8, 8, 1)]
void main(uint3 DTid: SV_DispatchThreadID) {
    uint2 pixel_coord = DTid.xy;
    if (pixel_coord.x >= resolution.x || pixel_coord.y >= resolution.y) return;

    float side = min(resolution.x, resolution.y);
    float2 center = resolution * 0.5;
    float2 square_half = float2(side, side) * 0.5;
    float2 square_min = center - square_half;
    float2 square_max = center + square_half;

    if (pixel_coord.x < square_min.x || pixel_coord.x >= square_max.x ||
        pixel_coord.y < square_min.y || pixel_coord.y >= square_max.y) {
        out_tex[pixel_coord] = float4(0, 0, 0, 1);
        return;
    }

    float2 square_uv = (float2(pixel_coord) - square_min) / side;
    float2 p = square_uv * 2.0 - 1.0;

    float2 uv = (float2)pixel_coord / resolution;
//  float2 p = uv * 2.0 - 1.0;

    float overlay_alpha = 0.0;

    // Main circle -- mainly aesthetic as the scope will not be able to reach it
    float circle_radius = 0.9;
    float circle_dist = abs(circle_sdf(p, float2(0, 0), circle_radius)) - line_thickness * 0.5;
    float circle_alpha = 1.0 - smoothstep(line_thickness * 0.2, line_thickness, circle_dist);
    overlay_alpha = max(overlay_alpha, circle_alpha);

    // Skintone & Q lines
    float2 skintone_end = skintone_dir * circle_radius;
    float2 Q_start = float2(skintone_dir.y, -skintone_dir.x) * circle_radius;
    float2 Q_end = -float2(skintone_dir.y, -skintone_dir.x) * circle_radius;
    float skintone_dist = line_sdf(p, float2(0, 0), skintone_end, line_thickness);
    float Q_dist = line_sdf(p, Q_start, Q_end, line_thickness);
    float main_lines_alpha = 1.0 - smoothstep(line_thickness * 0.2, line_thickness, skintone_dist) +
                             1.0 - smoothstep(line_thickness * 0.2, line_thickness, Q_dist);
    overlay_alpha = max(overlay_alpha, main_lines_alpha);

    // Boxes -- to indicate max saturation
    for (int i = 0; i < 6; ++i) {
        float2 cbcr = float2(dot(main_colors[i], RGB_to_Cb), dot(main_colors[i], RGB_to_Cr));
        cbcr *= 2.0 * scope_scale;
        float outer_box = box_sdf(p, cbcr, float2(box_size, box_size));
        float inner_box = box_sdf(p, cbcr, float2(box_size - line_thickness * 2.0, box_size - line_thickness * 2.0));
        float box_dist = max(outer_box, -inner_box);
        float box_alpha = 1.0 - smoothstep(-line_thickness * 0.1, line_thickness, box_dist);
        overlay_alpha = max(overlay_alpha, box_alpha);
    }

    // Finalize the overlay by giving it a color (for now uniform)
    overlay_alpha = saturate(overlay_alpha);
    float3 overlay = overlay_alpha * overlay_color;

    // Load vectorscope intensity
    float2 texSize;
    vs_blur_tex.GetDimensions(texSize.x, texSize.y);
    float2 scaled_sq_uv = (square_uv - 0.5) / scope_scale + 0.5;
    int2 texel = int2(scaled_sq_uv * texSize);
    float v = vs_blur_tex.Load(int3(texel, 0));
    // HACK: Adding this as hardcoded for now, as this should be the resolution of the accumulator texture
    // NOT the composite texture
    float v_max = 1024.0 * 1024.0;
    float intensity = log(1.0 + v) / log(1.0 + v_max) * 8.0;
    
    // Calculate how to color the current pixel
    float Cb = square_uv.x - 0.5;
    float Cr = square_uv.y - 0.5;
    float3 ycbcr_color = float3(0.5, Cb, Cr);
    float3 rgb_color = ycbcr_to_rgb(ycbcr_color);

    float3 final_color = (rgb_color * intensity) + overlay;
    out_tex[pixel_coord] = float4(final_color, 1.0);
}

