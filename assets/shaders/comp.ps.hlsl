Texture2D<float> tex : register(t0);
Texture2D ui : register(t1);
SamplerState samp : register(s0);

struct PSInput {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float3 LinearToSRGB_Polynomial(float3 color) {
    color = saturate(color);

    // Optimized polynomial approximation that's very close to correct sRGB
    // Coefficients chosen for minimal error in typical color ranges
    float3 S1 = sqrt(color);
    float3 S2 = sqrt(S1);
    float3 S3 = sqrt(S2);

    return 0.585122381f * S1 + 0.783140355f * S2 - 0.368262736f * S3;
}

float3 ACESFilm(float3 x) {
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float aces(float x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float3 YCbCrToRGB(float3 ycbcr) {
    float Y = ycbcr.x;
    float Cb = ycbcr.y;
    float Cr = ycbcr.z;

    float r = Y + 1.402 * Cr;
    float g = Y - 0.344136 * Cb - 0.714136 * Cr;
    float b = Y + 1.772 * Cb;

    return saturate(float3(r, g, b));
}

float ring(float r, float radius) {
    return smoothstep(0.003, 0.0, abs(r - radius));
}

float circleSDF(float2 p, float2 center, float radius) {
    return length(p - center) - radius;
}

float lineSDF(float2 p, float2 a, float2 b, float thickness) {
    float2 pa = p - a;
    float2 ba = b - a;
    float h = saturate(dot(pa, ba) / dot(ba, ba));
    return length(pa - ba * h) - thickness * 0.5;
}

float boxSDF(float2 p, float2 center, float2 size) {
    float2 d = abs(p - center) - size * 0.5;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

static const float3 RGB_to_Cb = float3(-0.1146, -0.3854, 0.5);
static const float3 RGB_to_Cr = float3(0.5, -0.4542, -0.0458);

#define UINT_MAX 1.0 / 42.94967295
#define SKINTONE_ANGLE radians(-123.0)
#define SKINTONE_DIR float2(cos(SKINTONE_ANGLE), sin(SKINTONE_ANGLE))
#define SCOPE_SCALE 0.6

float4 main(PSInput input) : SV_TARGET {
    float2 center = float2(0.5, 0.5);
    float2 scaled_uv = (input.uv - center) / SCOPE_SCALE + center;

    float2 p = input.uv * 2.0 - 1.0;
    float overlay_alpha = 0.0;
    float3 overlay_color = float3(0.71f, 0.57f, 0.16f) * 0.5;
    float line_thickness = 0.005;

    // Main outside circle
    float circle_radius = 0.99;
    float circle_dist = abs(circleSDF(p, float2(0, 0), circle_radius)) - line_thickness * 0.5;
    float circle_alpha = 1.0 - smoothstep(line_thickness * 0.2, line_thickness, circle_dist);
    overlay_alpha = max(overlay_alpha, circle_alpha);

    // Skintone & Q lines
    float2 skintone_end = SKINTONE_DIR * circle_radius;
    float2 Q_start = float2(SKINTONE_DIR.y, -SKINTONE_DIR.x) * circle_radius;
    float2 Q_end = -float2(SKINTONE_DIR.y, -SKINTONE_DIR.x) * circle_radius;
    float skintone_dist = lineSDF(p, float2(0, 0), skintone_end, line_thickness);
    float Q_dist = lineSDF(p, Q_start, Q_end, line_thickness);
    float main_lines_alpha = 1.0 - smoothstep(line_thickness * 0.2, line_thickness, skintone_dist) +
                             1.0 - smoothstep(line_thickness * 0.2, line_thickness, Q_dist);
    overlay_alpha = max(overlay_alpha, main_lines_alpha);

    // Boxes
    float box_size = 0.1;
    float3 colors[6] = {float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), float3(0.0, 0.0, 1.0), float3(1.0, 1.0, 0.0), float3(0.0, 1.0, 1.0), float3(1.0, 0.0, 1.0)};
    for (int i = 0; i < 6; ++i) {
        float3 col_lin = LinearToSRGB_Polynomial(colors[i]);
        float2 cbcr = float2(dot(col_lin, RGB_to_Cb), -dot(col_lin, RGB_to_Cr));
        cbcr *= 2.0 * SCOPE_SCALE;
        float outer_box = boxSDF(p, cbcr, float2(box_size, box_size));
        float inner_box = boxSDF(p, cbcr, float2(box_size - line_thickness * 2.0, box_size - line_thickness * 2.0));
        float box_dist = max(outer_box, -inner_box);
        float box_alpha = 1.0 - smoothstep(-line_thickness * 0.1, line_thickness, box_dist);
        overlay_alpha = max(overlay_alpha, box_alpha);
    }

    overlay_alpha = saturate(overlay_alpha);
    float3 overlay = overlay_alpha * overlay_color;

    float3 ui_tex = ui.Sample(samp, input.uv).rgb;

    uint2 texSize;
    tex.GetDimensions(texSize.x, texSize.y);
    int2 pixelCoord = int2(scaled_uv * float2(texSize));
    pixelCoord = clamp(pixelCoord, int2(0, 0), int2(texSize) - 1);
    float v = tex.Load(int3(int2(pixelCoord), 0));

    float v_max = texSize.x * texSize.y;
    float brightness = log(1.0 + v) / log(1.0 + v_max) * 8.0;

    float Cb = scaled_uv.x - 0.5;
    float Cr = (1.0 - scaled_uv.y) - 0.5;

    float3 ycbcr_hue = float3(0.5, Cb, Cr);
    float3 hue_color = YCbCrToRGB(ycbcr_hue);

    float4 scope_color = float4((hue_color * brightness), 1.0);

//    return scope_color + float4(overlay, 1.0);
    return float4(ui_tex, 1.0);
}

