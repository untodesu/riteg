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
    /* https://www.desmos.com/calculator/croqc9jxlt */
    const float texp = param_a.y * exp(-5.0 * param_a.z * uv.y * uv.y);
    const float thres = max(param_a.x - texp, 0.0);
    const float steps = param_a.w;

    float dvalue = 0.0;
    for(float i = 1.0; i <= steps; i += 1.0) {
        if(rand(uv.x + i / screen.x, uv.y) >= thres)
            dvalue = 0.0;
        else dvalue += 1.0;
    }

    const float noise = rand(uv.x, uv.y);
    const float nvalue = noise * (steps - dvalue) / steps;
    const vec4 color = texture(image, uv);

    target.x = mix(color.x, 1.0, nvalue);
    target.y = mix(color.y, 0.0, nvalue);
    target.z = mix(color.z, 0.0, nvalue);
    target.w = color.w;

/*
    const float fx = rand(uv.x, uv.y);
    const float fy = rand(fx, uv.y);
    const float fz = rand(fx, uv.y);
    const float pixel = 1.0 / screen.x;

    /* https://www.desmos.com/calculator/ff4qu2gf5w
    const float paxmap = 1.0 - param_a.x * 0.01;
    const float thx = param_a.z * (uv.y - 0.01 * param_a.w);
    const float thres = paxmap - 0.01 * param_a.y * thx * exp(1.0 - thx);
    const float steps = (1.0 - thres) * param_b.y + ceil(param_b.z * fx);

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

    const float ymod = 0.5 * rand(uv.y, noise);
    const float imod = ymod * (2.0 * fy - 1.0);
    const float qmod = ymod * (2.0 * fx - 1.0);

    target.x = noise;
    target.y = imod;
    target.z = qmod;
    target.w = 1.0
*/
}
