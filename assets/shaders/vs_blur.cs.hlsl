Texture2D<uint> src : register(t0);
RWTexture2D<float> dst : register(u0);

#define TEXSIZE 1024
#define RADIUS 2

[numthreads(8, 8, 1)]
void main(uint3 dtid: SV_DispatchThreadID) {
    int2 coord = int2(dtid.xy);
    if (coord.x >= TEXSIZE || coord.y >= TEXSIZE) return;

    float result = 0.0;
    int count = 0;

    [unroll]
    for (int y = -RADIUS; y <= RADIUS; ++y) {
        [unroll]
        for (int x = -RADIUS; x <= RADIUS; ++x) {
            if (abs(x) + abs(y) <= RADIUS) {
                int2 sample_coord = clamp(coord + int2(x, y), int2(0, 0), TEXSIZE - 1);
                uint raw = src.Load(int3(sample_coord, 0));
                result += float(raw);
                count += 1;
            }
        }
    }
    dst[coord] = result / count;
}


