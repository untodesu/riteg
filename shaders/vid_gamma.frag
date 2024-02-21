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
    const float gamma = 1.0 / param_a.x;
    const vec4 color = texture(image, uv);
    target.x = pow(color.x, gamma);
    target.y = pow(color.y, gamma);
    target.z = pow(color.z, gamma);
    target.w = pow(color.w, gamma);
}
