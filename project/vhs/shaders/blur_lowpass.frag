#if RITEG
#define FIR_SIZE    iParam[0]
#define FIR_CUTOFF  iParam[1]
#define FIR_GAIN    iParam[2]
#else
#define FIR_SIZE    64
#define FIR_CUTOFF  0.285
#define FIR_GAIN    1.0
#endif

float sinc(float x)
{
    if(x != 0.0) {
        float pi = atan(1.0) * 4.0;
        float pi_x = pi * x;
        return sin(pi_x) / pi_x;
    }

    return 1.0;
}

float blackman(int N, int i, float alpha)
{
    float A0 = 0.5 - 0.5 * alpha;
    float A1 = 0.5;
    float A2 = 0.5 * alpha;
    float tau = atan(1.0) * 8.0;
    float dtime = float(i) / float(N - 1);
    return A0 - A1 * cos(tau * dtime) + A2 * cos(2.0 * tau * dtime);
}

float blackman_harris(int N, int i)
{
    float A0 = 0.35875;
    float A1 = 0.48829;
    float A2 = 0.14128;
    float A3 = 0.01168;
    float pi = atan(1.0) * 4.0;
    float dt = float(i) / float(N - 1);
    return A0 - A1 * cos(2.0 * pi * dt) + A2 * cos(4.0 * pi * dt) - A3 * cos(6.0 * pi * dt);
}

float fir_lowpass(float sample_rate, float cutoff, int N, int i)
{
    float wc = cutoff / sample_rate;
    float window = blackman_harris(N, i);
    return 2.0 * wc * window * sinc(2.0 * wc * float(i - N / 2));
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = fragCoord.xy / iResolution.xy;
    float sample_rate = iResolution.x;
    float n = float(fragCoord.x);

    int filter_size = int(FIR_SIZE);
    float cutoff = sample_rate * FIR_CUTOFF * 0.5;

    fragColor.x = 0.0;
    fragColor.y = 0.0;
    fragColor.z = 0.0;
    fragColor.w = 0.0;

    for(int i = 0; i < filter_size; ++i) {
        int temp_idx = filter_size - i - 1;
        float coeff = fir_lowpass(sample_rate, cutoff, filter_size, temp_idx);
        ivec2 uvmod = ivec2(fragCoord) + ivec2(i - filter_size / 2, 0);
        fragColor += FIR_GAIN * coeff * texelFetch(iChannel0, uvmod, 0);
    }
}
