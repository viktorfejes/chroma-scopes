Texture2D ui : register(t1);
SamplerState samp : register(s0);

struct PSInput {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET {
    float3 ui_tex = ui.Sample(samp, input.uv).rgb;
    return float4(ui_tex, 1.0);
}

