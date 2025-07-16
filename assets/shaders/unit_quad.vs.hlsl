cbuffer PerFrameData : register(b0) {
    float4x4 projection;
};

cbuffer UIElementData : register(b1) {
    float2 position;
    float2 size;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

// TODO: Unit quad
static const float2 pos[3] = {
    float2(-1.0, -1.0),
    float2(-1.0,  3.0),
    float2( 3.0, -1.0)
};

VSOutput main(uint vertex_id : SV_VertexID) {
    VSOutput o;
    o.position = float4(pos[vertex_id], 0.0, 1.0);
    o.uv = (pos[vertex_id] + 1.0) * 0.5;
    o.uv.y = 1.0 - o.uv.y;
    return o;
}
