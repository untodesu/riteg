
#version 450 core

#define NTSC_FILTER_SIZE    29
#define NTSC_LINE_RATE      15750.0
#define NTSC_Y_CUTOFF       1000000.0
#define NTSC_IQ_CARRIER     3500000.0
#define NTSC_IQ_CUTOFF_L    2000000.0
#define NTSC_IQ_CUTOFF_H    4000000.0

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 target;

layout(binding = 0, std140) uniform params {
    vec4 param_a;
    vec4 param_b;
    vec4 param_c;
    vec4 param_d;
    vec4 screen;
    vec4 timing;
};

layout(binding = 0) uniform sampler2D signal;

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
    const uint px = uint(x * screen.x);
    const uint py = uint(y * screen.y);
    const uint pz = uint(timing.y * 1000.0);
    const uint hv = hash(px + hash(py) + hash(pz));
    return uintBitsToFloat(0x3F800000U | (hv >> 9)) - 1.0;
}

vec2 demodulate_qam(float fs, float carrier, float n, float sig)
{
    const float pi = atan(1.0) * 4.0;
    const float phase = 2.0 * pi * (carrier * (n / fs) + rand(uv.y, 0.0));
    
    const float cv = cos(phase);
    const float sv = sin(phase);
    const float ival = sig * cv;
    const float qval = sig * sv;
    
    return vec2(ival, qval);
}

float sinc(float x)
{
    if(x != 0.0) {
        const float pi = atan(1.0) * 4.0;
        const float pi_x = pi * x;
        return sin(pi_x) / pi_x;
    }

    return 1.0;
}

float blackman_harris(int N, int i)
{
    const float A0 = 0.35875;
    const float A1 = 0.48829;
    const float A2 = 0.14128;
    const float A3 = 0.01168;
    const float pi = atan(1.0) * 4.0;
    const float dtime = float(i) / float(N - 1);
    return A0 - A1 * cos(2.0 * pi * dtime) + A2 * cos(4.0 * pi * dtime) - A3 * cos(6.0 * pi * dtime);
}

float fir_lowpass(float fs, float fc, int N, int i)
{
    const float wc = fc / fs;
    const float window = blackman_harris(N, i);
    return 2.0 * wc * window * sinc(2.0 * wc * float(i - N / 2));
}

float fir_bandpass(float fs, float fcl, float fch, int N, int i)
{
    const float wcl = fcl / fs;
    const float wch = fch / fs;
    const float window = blackman_harris(N, i);
    const float vl = sinc(2.0 * wcl * float(i - N / 2));
    const float vh = sinc(2.0 * wch * float(i - N / 2));
    return 2.0 * (wch - wcl) * window * (vh - vl);
}

void main(void)
{
    const ivec2 pixcoord = ivec2(screen.xy * uv);
    const float n = floor(pixcoord.x);
    const float fs = screen.x * NTSC_LINE_RATE;

    float accum_y = 0.0;
    float accum_iq = 0.0;

    for(int i = 0; i < NTSC_FILTER_SIZE; ++i) {
        int tempindex = NTSC_FILTER_SIZE - i - 1;
        float lp = fir_lowpass(fs, NTSC_Y_CUTOFF, NTSC_FILTER_SIZE, tempindex);
        float bp = fir_bandpass(fs, NTSC_IQ_CUTOFF_L, NTSC_IQ_CUTOFF_H, NTSC_FILTER_SIZE, tempindex);
        float texel = texelFetch(signal, ivec2(n + float(NTSC_FILTER_SIZE / 2) - i, pixcoord.y), 0).x;
        accum_y += lp * texel;
        accum_iq += bp * texel;
    }

    target.x = accum_y;
    target.yz = demodulate_qam(fs, NTSC_IQ_CARRIER, n, accum_iq);
    target.w = 1.0;
}
