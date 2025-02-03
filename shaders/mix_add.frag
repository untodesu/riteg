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

layout(binding = 0) uniform sampler2D image_a;
layout(binding = 1) uniform sampler2D image_b;

void main(void)
{
    const vec4 a = texture(image_a, uv);
    const vec4 b = texture(image_b, uv);
    target = param_a.x * a + param_a.y * b;
}
