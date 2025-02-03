#if RITEG
#define P0 iParam[0]
#define P1 iParam[1]
#define P2 iParam[2]
#else
#define P0 70.0
#define P1 2.0
#define P2 0.125
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
    vec2 uv = fragCoord / iResolution.xy;
    float pixel = 1.0 / iResolution.x;

    float factor = exp(-0.5 * P0 * uv.y);
    float wave = P1 * rand(uv.y, 0.0);
    vec4 color = texture(iChannel0, uv + vec2(factor * wave * pixel, 0.0));

    float ymod = P2 * dot(color.yz, color.yz);
    float imod = ymod * (2.0 * rand(0.5 * uv.y, 0.0) - 1.0);
    float qmod = ymod * (2.0 * rand(0.0, 0.5 * uv.y) - 1.0);

    fragColor.x = color.x;
    fragColor.y = mix(color.y, imod, factor);
    fragColor.z = mix(color.z, qmod, factor);
    fragColor.w = color.w;
}
