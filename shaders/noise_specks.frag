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
    const float noise = rand(5.0 * uv.x, 5.0 * uv.y);
    const float nfx = 0.6 * rand(noise, uv.x) + 0.7;
    const float nfy = 0.6 * rand(noise, uv.y) + 0.7;
    const float nfz = 0.6 * rand(nfx, nfy) + 0.7;
    const float thres = 1.0 - param_a.x;
    const float value = step(thres, noise);
    target.x = value * nfx;
    target.y = value * nfy;
    target.z = value * nfz;
    target.w = value;
}
