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
    const float pixel = 1.0 / screen.x;

    const float factor = exp(-0.5 * param_a.x * uv.y);
    const float wave = param_a.y * rand(uv.y, 0.0);
    const vec4 color = texture(image, uv + vec2(factor * wave * pixel, 0.0));

    const float ymod = param_a.z * dot(color.yz, color.yz);
    const float imod = ymod * (2.0 * rand(0.5 * uv.y, 0.0) - 1.0);
    const float qmod = ymod * (2.0 * rand(0.0, 0.5 * uv.y) - 1.0);

    target.x = color.x;
    target.y = mix(color.y, imod, factor);
    target.z = mix(color.z, qmod, factor);
    target.w = color.w;
}
