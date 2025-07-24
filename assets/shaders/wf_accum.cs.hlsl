// Texture2D<float4> input_tex : register(t0);
// RWTexture2D<uint4> output_tex : register(u0);
// 
// static const uint BUCKETS = 512;
// 
// [numthreads(8, 8, 1)]
// void main(uint3 DTid : SV_DispatchThreadID) {
//     uint2 in_dim;
//     input_tex.GetDimensions(in_dim.x, in_dim.y);
//     
//     if (DTid.x >= in_dim.x || DTid.y >= in_dim.y) return;
//     
//     float3 pixel = input_tex[DTid.xy].rgb;
// 
//     // Calculate luma (rec.709 for now)
//     float luma = dot(pixel, float3(0.2126, 0.7152, 0.0722));
// 
//     // Compute bucket index
//     uint bucket_r = clamp((uint)(pixel.r * BUCKETS), 0, BUCKETS - 1);
//     uint bucket_g = clamp((uint)(pixel.g * BUCKETS), 0, BUCKETS - 1);
//     uint bucket_b = clamp((uint)(pixel.b * BUCKETS), 0, BUCKETS - 1);
//     uint bucket_l = clamp((uint)(luma * BUCKETS), 0, BUCKETS - 1);
// 
//     // Use x coordinate of input pixel as the column in the output texture
//     uint out_x = DTid.x;
// 
//     InterlockedAdd(output_tex[uint2(out_x, bucket_r)].r, 1);
//     InterlockedAdd(output_tex[uint2(out_x, bucket_g)].g, 1);
//     InterlockedAdd(output_tex[uint2(out_x, bucket_b)].b, 1);
//     InterlockedAdd(output_tex[uint2(out_x, bucket_l)].a, 1);
// }

Texture2D<float4> input_tex : register(t0);
RWStructuredBuffer<uint3> output_tex : register(u0);

static const uint BUCKETS = 512;
// TEMP: Hardcoding the output_tex's width here
static const uint out_width = 1024;

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    uint2 in_dim;
    input_tex.GetDimensions(in_dim.x, in_dim.y);

    if (DTid.x >= in_dim.x || DTid.y >= in_dim.y) return;

    // Read and clamp pixel to 0–1 range
    float3 pixel = saturate(input_tex[uint2(DTid.x, DTid.y)].rgb);

    // Calculate luma (rec.709 for now)
    float luma = dot(pixel, float3(0.2126, 0.7152, 0.0722));

    // Compute vertical bucket index for each channel
    uint bucket_r = clamp((uint)(pixel.r * BUCKETS), 0, BUCKETS - 1);
    uint bucket_g = clamp((uint)(pixel.g * BUCKETS), 0, BUCKETS - 1);
    uint bucket_b = clamp((uint)(pixel.b * BUCKETS), 0, BUCKETS - 1);
    uint bucket_l = clamp((uint)(luma * BUCKETS), 0, BUCKETS - 1);

    // Compute range of output X pixels this input pixel maps to
    float x_scale = float(out_width) / float(in_dim.x);
    float out_x_start = DTid.x * x_scale;
    float out_x_end   = (DTid.x + 1) * x_scale;

    for (uint x = (uint)floor(out_x_start); x < (uint)ceil(out_x_end); ++x) {
        if (x < out_width) {
            InterlockedAdd(output_tex[x + bucket_r * out_width].r, 1);
            InterlockedAdd(output_tex[x + bucket_g * out_width].g, 1);
            InterlockedAdd(output_tex[x + bucket_b * out_width].b, 1);
        }
    }
}

