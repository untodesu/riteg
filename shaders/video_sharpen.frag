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
    const vec2 step = param_a.xy / screen.xy;
    target = vec4(0.0, 0.0, 0.0, 0.0);
    target -= texture(image, uv + vec2(step.x, 0.0));
    target -= texture(image, uv + vec2(0.0, step.y));
    target -= texture(image, uv - vec2(step.x, 0.0));
    target -= texture(image, uv - vec2(0.0, step.x));
    target += 5.0 * texture(image, uv);
}
