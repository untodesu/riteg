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
    const float dt = float(i) / float(N - 1);
    return A0 - A1 * cos(2.0 * pi * dt) + A2 * cos(4.0 * pi * dt) - A3 * cos(6.0 * pi * dt);
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
    const float fs = screen.x * NTSC_LINE_RATE;

    target.x = 0.0;
    target.y = 0.0;
    target.z = 0.0;
    target.w = 1.0;

#if 1
    for(int i = 0; i < NTSC_FILTER_SIZE; ++i) {
        int idx = NTSC_FILTER_SIZE - i - 1;
        float lp = fir_lowpass(fs, NTSC_Y_CUTOFF, NTSC_FILTER_SIZE, idx);
        ivec2 uvmod = pixcoord + ivec2(i - NTSC_FILTER_SIZE / 2, 0);
        target.yz += lp * texelFetch(image, uvmod, 0).yz;
    }
#else
    target.yz = texelFetch(image, pixcoord, 0).yz;
#endif

    target.x = texelFetch(image, pixcoord, 0).x;

    target.x *= param_a.x;
    target.y *= param_a.y;
    target.z *= param_a.y;

#if 0
    target.y *= -1.0;

    // NTSC stands for:
    //  N - Never
    //  T - The
    //  S - Same
    //  C - Color
    // https://gist.github.com/mairod/a75e7b44f68110e1576d77419d608786
    const float chrominance = length(target.yz);
    const float hue = atan(target.y, target.z) + param_a.z * 0.01745329251;
    target.y = chrominance * cos(hue);
    target.z = chrominance * sin(hue);
#endif
}
