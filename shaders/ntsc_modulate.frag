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

float modulate_qam(float fs, float carrier, float n, vec2 sig)
{
    const float pi = atan(1.0) * 4.0;
    const float phase = 2.0 * pi * carrier * (n / fs);
    return dot(sig, vec2(cos(phase), sin(phase)));
}

void main(void)
{
    const ivec2 pixcoord = ivec2(screen.xy * uv);
    const vec4 color = texture(image, uv);
    const float n = float(pixcoord.x);

    const float fs = screen.x;
    const float iq_carrier = fs * NTSC_IQ_CARRIER;

    target.x = color.x + modulate_qam(fs, iq_carrier, n, color.yz);
}
