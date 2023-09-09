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
    const float steps = 4.0 + ceil(8.0 * rand(uv.y, 0.0));
    const float thres = 1.0 - exp(-param_a.x * uv.y - param_a.y);

    float accum = step(thres, rand(uv.x, uv.y));
    for(float i = 1.0; i <= steps; ++i)
        accum += step(thres, rand(uv.x - i * pixel, uv.y)) / i * 6.0;
    accum /= steps;

    target.x = accum;
    target.y = accum;
    target.z = accum;
    target.w = accum;
}
