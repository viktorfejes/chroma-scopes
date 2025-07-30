StructuredBuffer<uint3> in_tex : register(t0);
RWTexture2D<float4> out_tex : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 DTid: SV_DispatchThreadID) {
    // TEMP:
    uint2 resolution = uint2(1024, 512);

    uint2 out_res;
    out_tex.GetDimensions(out_res.x, out_res.y);

    uint2 pixel_coord = DTid.xy;
    if (DTid.x >= out_res.x || DTid.y >= out_res.y) return;

    uint channel_width = out_res.x / 3;
    uint channel_index = DTid.x / channel_width;

    uint local_x = DTid.x % channel_width;

    uint2 input_uv = uint2(
        uint((float(local_x) + 0.5f) * (float(resolution.x) / float(channel_width))),
        uint((float(DTid.y) + 0.5f) * (float(resolution.y) / float(out_res.y))));

    input_uv.x = min(input_uv.x, resolution.x - 1);
    input_uv.y = min(input_uv.y, resolution.y - 1);
    uint buffer_idx = input_uv.y * resolution.x + input_uv.x;

    uint3 input_color = in_tex[buffer_idx];
    float3 intensity = (log(1.0 + input_color) / log(1.0 + resolution.y)) * 1.25;

    float4 result = 0.0;
    if (channel_index == 0)
        result = float4(intensity.r, 0, 0, 1);
    else if (channel_index == 1)
        result = float4(0, intensity.g, 0, 1);
    else
        result = float4(0, 0, intensity.b, 1);

    out_tex[DTid.xy] = result;
}

