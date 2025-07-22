Texture2D<float4> input_tex : register(t0);
RWTexture2D<float4> output_tex : register(u0);

static const float MAX_VALUE = 270.0;
static const float BUCKETS = 512.0;
static const float GAIN = 0.3;
static const float BLEND = 0.6;

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    // Get both input and output dimensions
    uint2 in_dim, out_dim;
    input_tex.GetDimensions(in_dim.x, in_dim.y);
    output_tex.GetDimensions(out_dim.x, out_dim.y);
    
    // Early exit if beyond output bounds
    if (DTid.x >= out_dim.x || DTid.y >= out_dim.y) return;
    
    // Initialize with black background
    float4 result = float4(0, 0, 0, 1);
    
    // Normalize output coordinates to 0-1 range
    float2 out_uv = float2(DTid.xy) / float2(out_dim);
    // Invert Y for correct orientation (0 = white at top, 1 = black at bottom)
    out_uv.y = 1.0 - out_uv.y;
    
    // Calculate bucket range for this pixel (based on normalized y position)
    float bucket_min = MAX_VALUE * floor(out_uv.y * BUCKETS) / BUCKETS;
    float bucket_max = MAX_VALUE * floor((out_uv.y * BUCKETS) + 1.0) / BUCKETS;
    
    // Initialize counters for R,G,B and luma
    float4 count = 0.0;
    
    // Sample and count values that fall within the bucket
    for(int i = 0; i < BUCKETS; i++) {
        // Normalize sample position
        float2 sample_uv = float2(out_uv.x, (float)i / BUCKETS);
        
        // Convert to input texture coordinates
        uint2 sample_pos = uint2(sample_uv.x * in_dim.x, sample_uv.y * in_dim.y);
        float4 pixel = input_tex[sample_pos] * 256.0;
        
        // Calculate Rec.709 luma
        pixel.a = dot(pixel.rgb, float3(0.2126, 0.7152, 0.0722));
        
        if(pixel.r >= bucket_min && pixel.r < bucket_max) count.r += 1.0;
        if(pixel.g >= bucket_min && pixel.g < bucket_max) count.g += 1.0;
        if(pixel.b >= bucket_min && pixel.b < bucket_max) count.b += 1.0;
        if(pixel.a >= bucket_min && pixel.a < bucket_max) count.a += 1.0;
    }
    
    // Apply log intensity scaling and luma blend
    float3 blended = lerp(count.rgb, count.aaa, BLEND);
    // Keep values in a range suitable for tonemapping
    result.rgb = log(max(blended, 1.0)) * GAIN;
    
    output_tex[DTid.xy] = result;
}

//#define HIGH_QUALITY

#ifndef HIGH_QUALITY
const int hres = 200;
const float intensity = 0.04;
const float thres = 0.004;
#else
const int hres = 800;
const float intensity = 0.03;
const float thres = 0.001;
#endif

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
	vec2 uv = fragCoord.xy / iResolution.xy;
    vec3 col = vec3(0);
    float s = uv.y*1.8 - 0.15;
    float maxb = s+thres;
    float minb = s-thres;
    
    for (int i = 0; i <= hres; i++) {
        vec3 x = texture(iChannel0, vec2(float(i)/float(hres), uv.x)).rgb;
		col += vec3(intensity)*step(x, vec3(maxb))*step(vec3(minb), x);

		float l = dot(x, x);
		col += vec3(intensity)*step(l, maxb*maxb)*step(minb*minb, l);
    }

	fragColor = vec4(col,1.0);
}
