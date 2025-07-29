Texture2D background_image : register(t0);
SamplerState linear_sampler : register(s0);

cbuffer PerObjectData : register(b1) {
    float2 position;
    float2 size;
    float2 uv_offset;
    float2 uv_scale;
    float4 background_color;
};

struct PSInput {
    float4 pos  : SV_POSITION;
    float2 uv   : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET {
    float4 image_color = background_image.Sample(linear_sampler, input.uv);
    return float4(background_color * (1.0 - image_color.a) + image_color);
}
