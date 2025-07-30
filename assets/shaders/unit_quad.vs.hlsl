cbuffer PerFrameData : register(b0) {
    float4x4 projection;
};

cbuffer PerObjectData : register(b1) {
    float2 position;
    float2 size;
    float2 uv_offset;
    float2 uv_scale;
    float4 color;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

static const float2 pos[6] = {
    float2(-0.5, -0.5),
    float2(-0.5,  0.5),
    float2( 0.5,  0.5),

    float2(-0.5, -0.5),
    float2( 0.5,  0.5),
    float2( 0.5, -0.5)
};

VSOutput main(uint vertex_id : SV_VertexID) {
    VSOutput o;

    // Get base quad vertex
    float2 local_pos = pos[vertex_id];

    // Scale and offset into object space
    float2 world_pos = local_pos * size + position;

    // Apply projection to get into NDC
    o.position = mul(projection, float4(world_pos, 0.0, 1.0));

    // UV
    float2 base_uv = local_pos + 0.5;
//  base_uv.y = 1.0 - base_uv.y;
    o.uv = uv_offset + base_uv * uv_scale;
    
    return o;
}
