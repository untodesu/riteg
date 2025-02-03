#if RITEG
#define P0 iParam[0]
#define P1 iParam[1]
#define P2 iParam[2]
#define P3 iParam[3]
#else
#define P0 0.003125
#define P1 30.00000
#define P2 3.000000
#define P3 0.062500
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

    if(uv.y <= P0) {
        float amount = 1.0 / exp((P0 - uv.y) * 10.0 * P1 + P2);
        float kjig = 1.0 / exp((P0 - uv.y) * P1 + P2 - 0.5);

        float jiggle = P3 * kjig * rand(0.0, uv.y);
        vec2 uvmod = vec2(uv.x - amount + jiggle, uv.y);

        if((uvmod.x < 0.0) || (uvmod.x > 1.0))
            fragColor = vec4(0.0, 0.0, 0.0, 1.0);
        else fragColor = texture(iChannel0, uvmod);

        fragColor.yz *= 1.0 - 25.0 * kjig;
    }
    else {
        fragColor = texture(iChannel0, uv);
    }
}
