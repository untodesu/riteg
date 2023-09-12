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
    const float noise = rand(0.0, uv.y);

    if(uv.y * screen.y > param_a.x) {
        const vec2 uvmod = vec2(uv.x - param_a.y * pixel * noise, uv.y);
        target = texture(image, uvmod);
    }
    else {
        const vec2 uvmod = vec2(uv.x - param_a.z * pixel * noise, uv.y);
        const vec4 color = texture(image, uvmod);
        target = vec4(color.x, 0.0, 0.0, color.w);
    }
}
