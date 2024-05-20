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

float modulate_qam(float fs, float carrier, float n, vec2 sig)
{
    const float pi = atan(1.0) * 4.0;
    const float shift = pi * uv.y * screen.y;
    const float phase = 2.0 * pi * carrier * (n / fs) + shift;
    return dot(sig, vec2(cos(phase), sin(phase)));
}

void main(void)
{
    const ivec2 pixcoord = ivec2(screen.xy * uv);
    const vec4 color = texture(image, uv);
    const float n = float(pixcoord.x);
    const float fs = screen.x * NTSC_LINE_RATE;
    target.x = color.x + modulate_qam(fs, NTSC_IQ_CARRIER, n, color.yz);
}
