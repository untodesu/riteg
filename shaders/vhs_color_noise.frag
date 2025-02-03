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
    const vec4 color = texture(image, uv);

    /* https://www.desmos.com/calculator/npwr0ngspa */
    const float h = param_a.z * (color.x + param_a.w);
    const float f = param_a.x + param_a.y * h * exp(1.0 - h);
    const float ru = rand(uv.x, uv.y);
    const float rv = rand(uv.y, uv.x);
    const float rx = rand(ru, rv);
    const float ry = rand(rv, ru);
    const float nx = rx * step(0.95, rx);
    const float ny = ry * step(0.95, ry);

    target.y = color.y + f * nx;
    target.z = color.z + f * ny;
    target.xw = color.xw;
}
