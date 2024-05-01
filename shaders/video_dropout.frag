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

layout(binding = 0) uniform sampler2D signal;

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
    const ivec2 pixcoord = ivec2(uv * screen.xy);
    const float value = texelFetch(signal, pixcoord, 0).x;

    const float fx = rand(uv.x, uv.y);
    const float fy = rand(fx, uv.y);
    const float fz = rand(fx, uv.y);

    /* https://www.desmos.com/calculator/ff4qu2gf5w */
    const float paxmap = 1.0 - param_a.x * 0.01;
    const float thx = param_a.z * (uv.y - 0.01 * param_a.w);
    const float thres = paxmap - 0.01 * param_a.y * thx * exp(1.0 - thx);
    const float steps = param_b.y + ceil(param_b.z * fx);

    float noise = 0.0;
    for(float i = 1.0; i <= steps; i += 1.0)
        noise += step(thres, rand(uv.x + i * pixel, uv.y)) * i;
    noise = clamp(noise / steps, 0.0, 1.0);

    const float linoise = rand(sin(noise), uv.y);
    const float linthres = 1.0 - param_b.x * 0.01;

    if(linoise >= linthres) {
        noise = 1.0 - noise;
        noise *= rand(uv.x, linoise);
    }

    target.x = value + noise;

/*
    float noise = step(thres, rand(uv.y, uv.x));
    for(float i = 1.0; i <= steps; i += 1.0)
        noise += step(thres, rand(uv.x + i * pixel, uv.y)) * i;
    noise = clamp(noise / steps, 0.0, 1.0);

    target.x = value + 0.5 * rand(uv.y, noise);
*/
}
