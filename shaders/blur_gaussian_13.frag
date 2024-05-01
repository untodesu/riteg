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
    const vec2 off1 = 1.411764705882353 * param_a.xy / screen.xy;
    const vec2 off2 = 3.294117647058823 * param_a.xy / screen.xy;
    const vec2 off3 = 5.176470588235294 * param_a.xy / screen.xy;
    target = texture(image, uv) * 0.1964825501511404;
    target += texture(image, uv + off1) * 0.2969069646728344;
    target += texture(image, uv - off1) * 0.2969069646728344;
    target += texture(image, uv + off2) * 0.09447039785044732;
    target += texture(image, uv - off2) * 0.09447039785044732;
    target += texture(image, uv + off3) * 0.010381362401148057;
    target += texture(image, uv - off3) * 0.010381362401148057;
}
