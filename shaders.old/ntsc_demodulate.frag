
#version 450 core

#define NTSC_Y_CUTOFF (1.0 / 8.0)
#define NTSC_IQ_CARRIER (1.0 / 4.0)
#define NTSC_IQ_BANDWIDTH (1.0 / 50.0)
#define NTSC_IQ_CUTOFF (1.0 / 8.0)
#define NTSC_FIR_SIZE 29

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

vec2 demodulate_qam(float fs, float carrier, float n, float sig)
{
    const float pi = atan(1.0) * 4.0;
    const float phase = 2.0 * pi * carrier * (n / fs) + uv.y * screen.y * 3.16555293987;
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

    const float fs = screen.x;
    const float iq_carrier = fs * NTSC_IQ_CARRIER;
    const float iq_bandwidth = fs * NTSC_IQ_BANDWIDTH;
    const float y_cutoff = fs * NTSC_Y_CUTOFF;

    const float fir_n = n + float(NTSC_FIR_SIZE / 2);
    const float fir_bpl = iq_carrier - iq_bandwidth;
    const float fir_bph = iq_carrier + iq_bandwidth;

    float accum_y = 0.0;
    float accum_iq = 0.0;

    for(int i = 0; i < NTSC_FIR_SIZE; ++i) {
        int tempindex = NTSC_FIR_SIZE - i - 1;
        float lp = fir_lowpass(fs, y_cutoff, NTSC_FIR_SIZE, tempindex);
        float bp = fir_bandpass(fs, fir_bpl, fir_bph, NTSC_FIR_SIZE, tempindex);
        float texel = texelFetch(signal, ivec2(fir_n - i, pixcoord.y), 0).x;
        accum_y += lp * texel;
        accum_iq += bp * texel;
    }

    target.x = accum_y;
    target.yz = demodulate_qam(fs, iq_carrier, n, accum_iq);
    target.w = 1.0;
}
