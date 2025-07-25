StructuredBuffer<uint3> in_tex : register(t0);
RWTexture2D<float4> out_tex : register(u0);

static const float line_thickness = 0.002;
static const float3 overlay_color = float3(0.71f, 0.57f, 0.16f) * 0.5;
static const float scope_scale = 0.6;
static const uint scope_line_count = 6;

float line_sdf(float2 p, float2 a, float2 b, float thickness) {
    float2 pa = p - a;
    float2 ba = b - a;
    float h = saturate(dot(pa, ba) / dot(ba, ba));
    return length(pa - ba * h) - thickness * 0.5;
}

[numthreads(8, 8, 1)]
void main(uint3 DTid: SV_DispatchThreadID) {
    // TEMP:
    float2 resolution = float2(1024, 512);

    uint2 pixel_coord = DTid.xy;
    if (pixel_coord.x >= resolution.x || pixel_coord.y >= resolution.y) return;

    float2 uv = pixel_coord / resolution;

    float overlay_alpha = 0.0;
    for (uint i = 0; i < scope_line_count; ++i) {
        float2 line_start = float2(0, (float)i / scope_line_count);
        float2 line_end = float2(1, (float)i / scope_line_count);
        float line_dist = line_sdf(pixel_coord / resolution, line_start, line_end, line_thickness);
        float line_alpha = 1.0 - smoothstep(line_thickness * 0.2, line_thickness, line_dist);
        overlay_alpha = max(overlay_alpha, line_alpha);
    }
    
    float3 overlay = overlay_alpha * overlay_color;

    // Waveform scope
    float3 color = in_tex[uint(DTid.x + DTid.y * resolution.x)];
    float3 intensity = (log(1.0 + color) / log(1.0 + resolution.y)) * 1.25;
    // intensity = color / resolution.y * 8.0; // This replaces the line above (not sure which one I prefer yet)

    out_tex[pixel_coord] = float4(intensity + overlay, 1.0);
}
