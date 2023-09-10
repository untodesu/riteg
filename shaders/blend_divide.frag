#version 450 core

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 target;

layout(binding = 1, std140) uniform uparams {
    vec4 param_a;
    vec4 param_b;
    vec4 param_c;
    vec4 param_d;
};

layout(binding = 0) uniform sampler2D a;
layout(binding = 1) uniform sampler2D b;

void main(void)
{
    target = texture(a, uv) * param_a.x;
    target /= texture(b, uv) * param_a.y;
}
