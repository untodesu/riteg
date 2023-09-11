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

void main(void)
{
    const vec2 pv = floor(screen.xy * vec2(uv.x, 1.0 - uv.y) / param_a.x);
    const float value = 1.0 - mod(pv.x + mod(pv.y, 2.0), 2.0);
    target.x = value;
    target.y = value;
    target.z = value;
    target.w = value;
}
