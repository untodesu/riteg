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
    const float off1x = 1.411764705882353 * param_a.x / screen.x;
    const float off1y = 1.411764705882353 * param_a.y / screen.y;
    const float off2x = 3.294117647058823 * param_a.x / screen.x;
    const float off2y = 3.294117647058823 * param_a.y / screen.y;
    const float off3x = 5.176470588235294 * param_a.x / screen.x;
    const float off3y = 5.176470588235294 * param_a.y / screen.y;
    const vec2 off1 = vec2(off1x, off1y);
    const vec2 off2 = vec2(off2x, off2y);
    const vec2 off3 = vec2(off3x, off3y);

    target = vec4(0.0, 0.0, 0.0, 0.0);
    target += texture(image, uv) * 0.1964825501511404;
    target += texture(image, uv + off1) * 0.2969069646728344;
    target += texture(image, uv - off1) * 0.2969069646728344;
    target += texture(image, uv + off2) * 0.09447039785044732;
    target += texture(image, uv - off2) * 0.09447039785044732;
    target += texture(image, uv + off3) * 0.010381362401148057;
    target += texture(image, uv - off3) * 0.010381362401148057;
}
