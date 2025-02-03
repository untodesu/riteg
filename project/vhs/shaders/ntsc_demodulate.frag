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

vec2 demodulate_qam(float fs, float carrier, float n, float sig, float y)
{
    float pi = atan(1.0) * 4.0;
    float phase = 2.0 * pi * (carrier * (n / fs) + rand(y, 0.0));
    
    float cv = cos(phase);
    float sv = sin(phase);
    float ival = sig * cv;
    float qval = sig * sv;
    
    return vec2(ival, qval);
}

float sinc(float x)
{
    if(x != 0.0) {
        float pi = atan(1.0) * 4.0;
        float pi_x = pi * x;
        return sin(pi_x) / pi_x;
    }

    return 1.0;
}

float blackman_harris(int N, int i)
{
    float A0 = 0.35875;
    float A1 = 0.48829;
    float A2 = 0.14128;
    float A3 = 0.01168;
    float pi = atan(1.0) * 4.0;
    float dtime = float(i) / float(N - 1);
    return A0 - A1 * cos(2.0 * pi * dtime) + A2 * cos(4.0 * pi * dtime) - A3 * cos(6.0 * pi * dtime);
}

float fir_lowpass(float fs, float fc, int N, int i)
{
    float wc = fc / fs;
    float window = blackman_harris(N, i);
    return 2.0 * wc * window * sinc(2.0 * wc * float(i - N / 2));
}

float fir_bandpass(float fs, float fcl, float fch, int N, int i)
{
    float wcl = fcl / fs;
    float wch = fch / fs;
    float window = blackman_harris(N, i);
    float vl = sinc(2.0 * wcl * float(i - N / 2));
    float vh = sinc(2.0 * wch * float(i - N / 2));
    return 2.0 * (wch - wcl) * window * (vh - vl);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = fragCoord.xy / iResolution.xy;
    float n = floor(fragCoord.x);
    float fs = NTSC_LINE_RATE * 1280.0;

    float accum_y = 0.0;
    float accum_iq = 0.0;

    for(int i = 0; i < NTSC_FILTER_SIZE; ++i) {
        int tempindex = NTSC_FILTER_SIZE - i - 1;
        float lp = fir_lowpass(fs, NTSC_Y_CUTOFF, NTSC_FILTER_SIZE, tempindex);
        float bp = fir_bandpass(fs, NTSC_IQ_CUTOFF_L, NTSC_IQ_CUTOFF_H, NTSC_FILTER_SIZE, tempindex);
        float texel = texelFetch(iChannel0, ivec2(n + float(NTSC_FILTER_SIZE / 2) - i, fragCoord.y), 0).x;
        accum_y += lp * texel;
        accum_iq += bp * texel;
    }

    fragColor.x = accum_y;
    fragColor.yz = demodulate_qam(fs, NTSC_IQ_CARRIER, n, accum_iq, uv.y);
    fragColor.w = 1.0;
}
