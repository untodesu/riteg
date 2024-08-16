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
    const vec4 color = texture(image, uv);
    target.x = color.x;//gamma_factor(color.x);
    target.y = 2.0 * param_a.x * atan(10.0 * param_a.y * color.y) / 3.14159265359;
    target.z = 2.0 * param_a.x * atan(10.0 * param_a.y * color.z) / 3.14159265359;
    target.w = color.w;//gamma_factor(color.w);
}
