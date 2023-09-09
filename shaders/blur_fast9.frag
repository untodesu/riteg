/* This is a port of a MIT licensed GLSL shader:
 * https://github.com/Jam3/glsl-fast-gaussian-blur */
#version 450 core

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 target;

layout(binding = 0, std140) uniform uargs {
    vec4 screen;
    vec4 timing;
};

layout(binding = 1, std140) uniform uparams {
    vec4 param_a;
    vec4 param_b;
    vec4 param_c;
    vec4 param_d;
};

layout(binding = 0) uniform sampler2D image;

void main(void)
{
    const float off1x = 1.3846153846 * param_a.x / screen.x;
    const float off1y = 1.3846153846 * param_a.y / screen.y;
    const float off2x = 3.2307692308 * param_a.x / screen.x;
    const float off2y = 3.2307692308 * param_a.y / screen.y;
    const vec2 off1 = vec2(off1x, off1y);
    const vec2 off2 = vec2(off2x, off2y);

    target = vec4(0.0, 0.0, 0.0, 0.0);
    target += texture(image, uv) * 0.2270270270;
    target += texture(image, uv + off1) * 0.3162162162;
    target += texture(image, uv - off1) * 0.3162162162;
    target += texture(image, uv + off2) * 0.0702702703;
    target += texture(image, uv - off2) * 0.0702702703;
}
