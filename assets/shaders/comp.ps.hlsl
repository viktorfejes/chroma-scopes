Texture2D tex     : register(t0);
Texture2D ui      : register(t1);
SamplerState samp : register(s0);

struct PSInput {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
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

float4 main(PSInput input) : SV_TARGET {
    float3 scope = LinearToSRGB_Polynomial(ACESFilm(tex.Sample(samp, input.uv).rgb));
    float3 ui_tex = ui.Sample(samp, input.uv).rgb;
    return float4(ui_tex, 1.0);
}
