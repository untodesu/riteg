#if RITEG
#define P0 iParam[0]
#define P1 iParam[1]
#define P2 iParam[2]
#define P3 iParam[3]
#define P4 iParam[4]
#else
#define P0 0.9999999
#define P1 0.000001
#define P2 50.0
#define P3 48.0
#define P4 0.999999
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

    /* https://www.desmos.com/calculator/croqc9jxlt */
    float texp = P1 * exp(-5.0 * P2 * uv.y * uv.y);
    float thres = max(P0 - texp, 0.0);
    float steps = P3;

    float dvalue = 0.0;
    for(float i = 1.0; i <= steps; i += 1.0) {
        if(rand(uv.x + i / iResolution.x, uv.y) >= thres)
            dvalue = 0.0;
        else dvalue += 1.0;
    }

    vec4 color = texture(iChannel0, uv);
    float noise = rand(uv.x, uv.y);
    float linoise = rand(0.0, uv.y);
    float linthres = P4;

    float nvalue = noise * (steps - dvalue) / steps * 2.0;

    if(linoise >= linthres) {
        nvalue = 1.0 - nvalue;
        nvalue += rand(uv.x, 0.0);
    }

    fragColor.x = mix(color.x, 1.0, nvalue);
    fragColor.y = mix(color.y, 0.0, nvalue);
    fragColor.z = mix(color.z, 0.0, nvalue);
    fragColor.w = color.w;
}
