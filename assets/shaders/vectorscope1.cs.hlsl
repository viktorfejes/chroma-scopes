Texture2D<float4> input_tex : register(t0);
RWTexture2D<uint> output_tex : register(u0);

static const float PI = 3.1415926535897932384626433832795;

static const int WIDTH = 1024;
static const int HEIGHT = 1024;
static const int CENTER = WIDTH / 2;

static const float3 RGB_to_Cb = float3(-0.1146, -0.3854, 0.5);
static const float3 RGB_to_Cr = float3(0.5, -0.4542, -0.0458);

[numthreads(16, 16, 1)]
void main(uint3 DTid: SV_DispatchThreadID) {
    uint2 dim;
    input_tex.GetDimensions(dim.x, dim.y);

    if (DTid.x >= dim.x || DTid.y >= dim.y) return;

    float4 color = input_tex[DTid.xy];
    float3 rgb = color.rgb;

    // Convert to CbCr
    float Cb = dot(rgb, RGB_to_Cb);
    float Cr = dot(rgb, RGB_to_Cr);

    int x = int((Cb + 0.5f) * WIDTH);
    int y = int((1.0f - (Cr + 0.5f)) * HEIGHT);

    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        InterlockedAdd(output_tex[int2(x, y)], 1);
    }
}

