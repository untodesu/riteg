#define NTSC_FILTER_SIZE    29
#define NTSC_LINE_RATE      15750.0
#define NTSC_Y_CUTOFF       1000000.0
#define NTSC_IQ_CARRIER     3500000.0
#define NTSC_IQ_CUTOFF_L    2000000.0
#define NTSC_IQ_CUTOFF_H    4000000.0

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

float modulate_qam(float fs, float carrier, float n, vec2 sig, float y)
{
    float pi = atan(1.0) * 4.0;
    float phase = 2.0 * pi * (carrier * (n / fs) + rand(y, 0.0));
    return dot(sig, vec2(cos(phase), sin(phase)));
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = fragCoord.xy / iResolution.xy;
    vec4 color = texture(iChannel0, uv);
    float n = float(fragCoord.x);
    float fs = NTSC_LINE_RATE * 1280.0;
    fragColor.x = color.x + modulate_qam(fs, NTSC_IQ_CARRIER, n, color.yz, uv.y);
}
