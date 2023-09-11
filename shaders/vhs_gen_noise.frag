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

uint hash(uint x)
{
    x ^= x >> 0x010U;
    x *= 0x7FEB352DU;
    x ^= x >> 0x010U;
    x *= 0x846CA68BU;
    x ^= x >> 0x010U;
    return x;
}

float rand(float x, float y)
{
    const uint px = uint(x * screen.x);
    const uint py = uint(y * screen.y);
    const uint pz = uint(timing.y * 1000.0);
    const uint hv = hash(px + hash(py) + hash(pz));
    return uintBitsToFloat(0x3F800000U | (hv >> 9)) - 1.0;
}

void main(void)
{
    const float thres = min(param_a.x, 1.0 - param_a.y * pow(sin(3.14159265359 * pow(1 - uv.y - param_a.z, 2.0 * param_a.w)), 2.0));
    const float steps = 4.0 + ceil(16 * rand(uv.y, uv.x));
    const float pixel = 1.0 / screen.x;

    float noise = 0.0;
    for(float i = 1.0; i <= steps; ++i)
        noise += step(thres, rand(uv.x - i * pixel, uv.y)) / i * 16.0;
    noise += step(thres, rand(uv.x, uv.y));
    noise /= steps;

    target.x = noise;
    target.y = noise;
    target.z = noise;
    target.w = noise;
}
