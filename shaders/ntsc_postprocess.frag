#version 450 core

#define NTSC_Y_CUTOFF (1.0 / 8.0)
#define NTSC_IQ_CARRIER (1.0 / 4.0)
#define NTSC_IQ_BANDWIDTH (1.0 / 50.0)
#define NTSC_IQ_CUTOFF (1.0 / 25.0)
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

layout(binding = 0) uniform sampler2D image;

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

void main(void)
{
    const ivec2 pixcoord = ivec2(screen.xy * uv);
    const float n = floor(pixcoord.x);

    const float fs = screen.x;
    const float iq_cutoff = fs * NTSC_IQ_CUTOFF;

    target.x = 0.0;
    target.y = 0.0;
    target.z = 0.0;
    target.w = 1.0;

#if 1
    for(int i = 0; i < NTSC_FIR_SIZE; ++i) {
        int idx = NTSC_FIR_SIZE - i - 1;
        float lp = fir_lowpass(fs, iq_cutoff, NTSC_FIR_SIZE, idx);
        ivec2 uvmod = pixcoord + ivec2(i - NTSC_FIR_SIZE / 2, 0);
        target.yz += lp * texelFetch(image, uvmod, 0).yz;
    }
#else
    target.yz = texelFetch(image, pixcoord, 0).yz;
#endif

    target.x = texelFetch(image, pixcoord, 0).x;

    target.x *= param_a.x;
    target.y *= param_a.y;
    target.z *= param_a.y;
}
