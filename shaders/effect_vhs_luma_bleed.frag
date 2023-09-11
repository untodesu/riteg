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
    const float pixel = 1.0 / screen.x;
    const float steps = ceil(param_a.x);
    target = vec4(0.0, 0.0, 0.0, 0.0);
    for(float i = 1.0; i < steps; ++i)
        target -= texture(image, uv - vec2(i * pixel, 0.0));
    target /= 2.0 * steps;
    target += param_a.y * texture(image, uv);
}
