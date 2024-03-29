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
    const float fx = rand(uv.x, uv.y);
    const float fy = rand(fx, uv.y);
    const float fz = rand(fx, uv.y);
    const float pixel = 1.0 / screen.x;
    const float steps = 8.0 + ceil(32.0 * fx);

    /* https://www.desmos.com/calculator/q1ynbyeyw2 */
    const float thx = param_a.z * (uv.y - param_a.w);
    const float thres = param_a.x - param_a.y * thx * exp(1.0 - thx);

    float noise = step(thres, rand(uv.y, uv.x));
    for(float i = 1.0; i <= steps; ++i)
        noise += step(thres, rand(uv.x - i * pixel, uv.y)) / i * 32.0 * fy;
    noise = clamp(noise / steps, 0.0, 1.0);

    const float linoise = rand(sin(noise), uv.y);

    if(linoise >= param_b.x) {
        noise = 1.0 - noise;
        noise *= rand(uv.x, linoise);
    }

    const float ymod = 0.5 * rand(uv.y, linoise);
    const float imod = ymod * (2.0 * fy - 1.0);
    const float qmod = ymod * (2.0 * fx - 1.0);

    target.x = noise;
    target.y = imod;
    target.z = qmod;
    target.w = 1.0;
}
