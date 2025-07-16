Texture2D<float4> input_tex : register(t0);
RWTexture2D<float4> output_tex : register(u0);

static const float PI = 3.1415926535897932384626433832795;
static const int WIDTH = 1024;
static const int HEIGHT = 1024;
static const int CENTER = WIDTH / 2;
// static const float3 RGB_to_Y = float3(0.2126, 0.7152, 0.0722);
static const float3 RGB_to_Cb = float3(-0.1146, -0.3854, 0.5);
static const float3 RGB_to_Cr = float3(0.5, -0.4542, -0.0458);

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    uint2 dim;
    input_tex.GetDimensions(dim.x, dim.y);
    
    if (DTid.x >= dim.x || DTid.y >= dim.y) return;
    
    float4 color = input_tex[DTid.xy];
    float3 rgb = color.rgb;
   
    // Convert to YCbCr
    //  float Y = dot(rgb, RGB_to_Y);
    float Cb = dot(rgb, RGB_to_Cb);
    float Cr = dot(rgb, RGB_to_Cr);
   
    // Calculate vectorscope coordinates
    const float scale = 0.8;
    const int scaledWidth = int(WIDTH * scale);
    const int scaledHeight = int(HEIGHT * scale);
    const float rotation = radians(-90.0);

    float len = length(float2(Cb, Cr));
    float angle = atan2(Cb, Cr) + rotation;

    int x = int(round(CENTER + scaledWidth * len * cos(angle)));
    int y = int(round(CENTER + scaledHeight * len * sin(angle)));

    // Only accumulate if within bounds
    // if (x >= 0 && x < WIDTH && y >= 0 && y < WIDTH) {
        // output_tex[int2(x, y)] += float4(rgb, 1.0);
    // }

    // Gaussian spread...
    const float spread = 1.5;
    // TEMP intensity
    const float intensity = 0.1;
    for(int dy = -1; dy <= 1; dy++) {
        for(int dx = -1; dx <= 1; dx++) {
            int2 spread_pos = int2(x + dx, y + dy);
            if(spread_pos.x >= 0 && spread_pos.x < WIDTH && 
            spread_pos.y >= 0 && spread_pos.y < WIDTH) {
                float weight = exp(-((dx*dx + dy*dy) / spread));
                output_tex[spread_pos] += float4(rgb * weight * intensity, weight);
            }
        }
    }
}
