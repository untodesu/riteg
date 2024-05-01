/* This is a port of a MIT licensed GLSL shader:
 * https://github.com/Jam3/glsl-fast-gaussian-blur */
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

void main(void)
{
    const vec2 off1 = 1.3846153846 * param_a.xy / screen.xy;
    const vec2 off2 = 3.2307692308 * param_a.xy / screen.xy;
    target = texture(image, uv) * 0.2270270270;
    target += texture(image, uv + off1) * 0.3162162162;
    target += texture(image, uv - off1) * 0.3162162162;
    target += texture(image, uv + off2) * 0.0702702703;
    target += texture(image, uv - off2) * 0.0702702703;
}
