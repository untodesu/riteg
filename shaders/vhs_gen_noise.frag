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

float rand(float x, float y)
{
    return fract(sin(dot(vec3(x, y, timing.y), vec3(12.9898, 78.233, 37.719))) * 143758.5453);
}

void main(void)
{
    const float pixel = 1.0 / screen.x;
    const float steps = ceil(param_a.x);

    float noise_accum = 0.0;
    for(float i = 1.0; i <= steps; ++i)
        noise_accum += step(param_a.y, rand(uv.x - i * pixel, uv.y)) / i * steps;
    noise_accum += step(param_a.y, rand(uv.x, uv.y));
    noise_accum /= steps;

    target.x = noise_accum;
    target.y = noise_accum;
    target.z = noise_accum;
    target.w = noise_accum;
}
