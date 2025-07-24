StructuredBuffer<uint3> in_tex : register(t0);
RWTexture2D<float4> out_tex : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 DTid: SV_DispatchThreadID) {
    // TEMP:
    float2 resolution = float2(1024, 512);

    uint2 pixel_coord = DTid.xy;
    if (pixel_coord.x >= resolution.x || pixel_coord.y >= resolution.y) return;

    float3 color = in_tex[uint(DTid.x + DTid.y * resolution.x)];
    float3 intensity = log(1.0 + color) / log(1.0 + resolution.y);
    intensity = color / resolution.y * 8.0; // This replaces the line above (not sure which one I prefer yet)

    out_tex[pixel_coord] = float4(intensity, 1.0);
}
