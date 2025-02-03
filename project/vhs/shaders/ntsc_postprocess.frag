#if RITEG
#define P0 iParam[0]
#define P1 iParam[1]
#define P2 iParam[2]
#else
#define P0 1.0
#define P1 2.5
#define P2 1.0
#endif

#define NTSC_FILTER_SIZE    29
#define NTSC_LINE_RATE      15750.0
#define NTSC_Y_CUTOFF       1000000.0
#define NTSC_IQ_CARRIER     3500000.0
#define NTSC_IQ_CUTOFF_L    2000000.0
#define NTSC_IQ_CUTOFF_H    4000000.0

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
    float dt = float(i) / float(N - 1);
    return A0 - A1 * cos(2.0 * pi * dt) + A2 * cos(4.0 * pi * dt) - A3 * cos(6.0 * pi * dt);
}

float fir_lowpass(float fs, float fc, int N, int i)
{
    float wc = fc / fs;
    float window = blackman_harris(N, i);
    return 2.0 * wc * window * sinc(2.0 * wc * float(i - N / 2));
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = fragCoord / iResolution.xy;
    float n = floor(fragCoord.x);
    float fs = NTSC_LINE_RATE * 640.0;

    fragColor.x = 0.0;
    fragColor.y = 0.0;
    fragColor.z = 0.0;
    fragColor.w = 1.0;

#if 1
    for(int i = 0; i < NTSC_FILTER_SIZE; ++i) {
        int idx = NTSC_FILTER_SIZE - i - 1;
        float lp = fir_lowpass(fs, NTSC_Y_CUTOFF, NTSC_FILTER_SIZE, idx);
        ivec2 uvmod = ivec2(fragCoord) + ivec2(i - NTSC_FILTER_SIZE / 2, 0);
        fragColor.yz += lp * texelFetch(iChannel0, uvmod, 0).yz;
    }
#else
    fragColor.yz = texelFetch(iChannel0, ivec2(fragCoord), 0).yz;
#endif

    fragColor.x = texelFetch(iChannel0, ivec2(fragCoord), 0).x;

    fragColor.x *= P0;
    fragColor.y *= P1;
    fragColor.z *= P1;

    fragColor.y *= -1.0;

    // NTSC stands for:
    //  N - Never
    //  T - The
    //  S - Same
    //  C - Color
    // https://gist.github.com/mairod/a75e7b44f68110e1576d77419d608786
    float chrominance = length(fragColor.yz);
    float hue = atan(fragColor.y, fragColor.z) + P2 * 0.01745329251;
    fragColor.y = chrominance * cos(hue);
    fragColor.z = chrominance * sin(hue);
}
