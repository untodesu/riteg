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

    const vec4 color = texture(image, uv);
    const float noise = rand(uv.x, uv.y);
    const float linoise = rand(0.0, uv.y);
    const float linthres = param_b.x;

    float nvalue = noise * (steps - dvalue) / steps * 2.0;

    if(linoise >= linthres) {
        nvalue = 1.0 - nvalue;
        nvalue += rand(uv.x, 0.0);
    }

    target.x = mix(color.x, 1.0, nvalue);
    target.y = mix(color.y, 0.0, nvalue);
    target.z = mix(color.z, 0.0, nvalue);
    target.w = color.w;
}
