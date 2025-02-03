#if RITEG
#define P0 iParam[0]
#define P1 iParam[1]
#define P2 iParam[2]
#define P3 iParam[3]
#else
#define P0 0.03125
#define P1 0.05
#define P2 50.0
#define P3 0.02
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
    vec4 color = texture(iChannel0, uv);

    /* https://www.desmos.com/calculator/npwr0ngspa */
    float h = P2 * (color.x + P3);
    float f = P0 + P1 * h * exp(1.0 - h);
    float ru = rand(uv.x, uv.y);
    float rv = rand(uv.y, uv.x);
    float rx = rand(ru, rv);
    float ry = rand(rv, ru);
    float nx = rx * step(0.95, rx);
    float ny = ry * step(0.95, ry);

    fragColor.y = color.y + f * nx;
    fragColor.z = color.z + f * ny;
    fragColor.xw = color.xw;
}
