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
    const vec2 off = 1.33333333 * param_a.xy / screen.xy;
    target = texture(image, uv) * 0.29411764705882354;
    target += texture(image, uv + off) * 0.35294117647058826;
    target += texture(image, uv - off) * 0.35294117647058826;
}
