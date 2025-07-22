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

#define UINT_MAX 1.0 / 42.94967295

float4 main(PSInput input) : SV_TARGET {
    float3 background = float3(0.0f, 0.0f, 0.0f);

    // Rings
    float2 uv = input.uv * 2.0 - 1.0;
    float r = length(uv);
    float rings = ring(r, 1.0);

    // Central crosshair
    float cross_thickness = 0.003;
    float2 centered = input.uv * 2.0 - 1.0;
    float cx = smoothstep(cross_thickness, 0.0, abs(centered.x));
    float cy = smoothstep(cross_thickness, 0.0, abs(centered.y));
    float cross = cx + cy;

    // Skin tone line
    float angle = atan2(uv.y, uv.x); // -PI to PI
    float skin_angle = radians(-123.0);
    float line_dist = 1.0;
    float line_thickness = 0.005;
    float2 line_dir = float2(cos(skin_angle), sin(skin_angle));
    float2 line_end = line_dir * line_dist;
    float proj_len = saturate(dot(uv, line_dir) / line_dist);
    float2 closest_point = line_dir * (proj_len * line_dist);
    float dist_to_line = length(uv - closest_point);
    float skin_line = 1.0 - smoothstep(0.0, line_thickness * 0.5, dist_to_line);

    background = (rings + cross + skin_line) * float3(0.71f, 0.57f, 0.16f) * 0.5f;

    float3 ui_tex = ui.Sample(samp, input.uv).rgb;

    uint2 texSize;
    tex.GetDimensions(texSize.x, texSize.y);
    int2 pixelCoord = int2(input.uv * float2(texSize));
    pixelCoord = clamp(pixelCoord, int2(0, 0), int2(texSize) - 1);
    float v = tex.Load(int3(int2(pixelCoord), 0));

    float v_max = texSize.x * texSize.y;
    float brightness = log(1.0 + v) / log(1.0 + v_max) * 8.0;

    float Cb = input.uv.x - 0.5;
    float Cr = (1.0 - input.uv.y) - 0.5;

    float3 ycbcr_hue = float3(0.5, Cb, Cr);
    float3 hue_color = YCbCrToRGB(ycbcr_hue);

    return float4((hue_color * brightness) + background, 1.0);
}



