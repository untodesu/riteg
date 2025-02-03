#if RITEG
#define P0 iParam[0]
#define P1 iParam[1]
#else
#define P0 0.2500
#define P1 0.0625
#endif

uint hash(uint x)
{
    x ^= x >> 0x010U;
    x *= 0x7FEB352DU;
    x ^= x >> 0x010U;
    x *= 0x846CA68BU;
    x ^= x >> 0x010U;
    return x;
}

float rand(float x, float y)
{
    uint px = uint(x * iResolution.x);
    uint py = uint(y * iResolution.y);
    uint pz = uint(iTime * 1000.0);
    uint hv = hash(px + hash(py) + hash(pz));
    return uintBitsToFloat(0x3F800000U | (hv >> 9)) - 1.0;
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = fragCoord.xy / iResolution.xy;
    float value = texelFetch(iChannel0, ivec2(fragCoord), 0).x;
    fragColor.x = value + (P0 + value) * P1 * rand(uv.x, uv.y);
}
