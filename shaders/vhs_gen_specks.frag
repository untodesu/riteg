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
    /* https://www.desmos.com/calculator/dgnxfkbt4l */
    const float thres = min(param_a.x, 1.0 - param_a.y * pow(sin(3.14159265359 * pow(1 - uv.y - param_a.z, 2.0 * param_a.w)), 2.0));
    const float noise = rand(5.0 * uv.x, 5.0 * uv.y);
    const float nfx = 0.6 * rand(noise, uv.x);
    const float nfy = 0.6 * rand(noise, uv.y);
    const float nfz = 0.6 * rand(nfx, nfy);
    const float value = step(thres, noise);
    target.r = value * nfx;
    target.g = value * nfy;
    target.b = value * nfz;
    target.a = value;
}
