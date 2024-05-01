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
    const vec2 step = 1.0 / screen.xy;
    const vec4 a = texture(image, uv + vec2(-step.x, -step.y) * 1.5);
    const vec4 b = texture(image, uv + vec2( step.x, -step.y) * 1.5);
    const vec4 c = texture(image, uv + vec2(-step.x,  step.y) * 1.5);
    const vec4 d = texture(image, uv + vec2(-step.x,  step.y) * 1.5);
    const vec4 around = 0.25 * (a + b + c + d);
    const vec4 center = texture(image, uv);
    target = clamp(center + (center - around) * param_a.x, 0.0, 1.0);
}
