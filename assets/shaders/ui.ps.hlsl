Texture2D background_image : register(t0);
SamplerState linear_sampler : register(s0);

cbuffer PerObjectData : register(b1) {
    float2 position;
    float2 size;
    float4 color;
};

struct PSInput {
    float4 pos  : SV_POSITION;
    float2 uv   : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET {
    return float4(color * background_image.Sample(linear_sampler, input.uv));
}
