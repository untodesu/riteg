#version 450 core

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 target;

layout(binding = 0, std140) uniform uargs {
    vec4 screen;
    vec4 timing;
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
    const float noise = rand(uv.x, uv.y);
    target.x = noise;
    target.y = noise;
    target.z = noise;
    target.w = noise;
}
