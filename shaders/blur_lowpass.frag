#version 450 core

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

float blackman(int N, int i, float alpha)
{
    const float A0 = 0.5 - 0.5 * alpha;
    const float A1 = 0.5;
    const float A2 = 0.5 * alpha;
    const float tau = atan(1.0) * 8.0;
    const float dtime = float(i) / float(N - 1);
    return A0 - A1 * cos(tau * dtime) + A2 * cos(2.0 * tau * dtime);
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

float fir_lowpass(float sample_rate, float cutoff, int N, int i)
{
    const float wc = cutoff / sample_rate;
    const float window = blackman_harris(N, i);
    return 2.0 * wc * window * sinc(2.0 * wc * float(i - N / 2));
}

void main(void)
{
    const float sample_rate = screen.x;
    const ivec2 pixcoord = ivec2(screen.xy * uv);
    const float n = floor(pixcoord.x);

    const int filter_size = int(param_a.x);
    const float cutoff = sample_rate * param_a.y * 0.5;
    const float gain = param_a.z;

    target.x = 0.0;
    target.y = 0.0;
    target.z = 0.0;
    target.w = 0.0;

    for(int i = 0; i < filter_size; ++i) {
        int temp_idx = filter_size - i - 1;
        float coeff = fir_lowpass(sample_rate, cutoff, filter_size, temp_idx);
        ivec2 uvmod = pixcoord + ivec2(i - filter_size / 2, 0);
        target += gain * coeff * texelFetch(image, uvmod, 0);
    }
}
