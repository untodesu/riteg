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
    if(uv.y <= param_a.x) {
        const float amount = 1.0 / exp((param_a.x - uv.y) * 10.0 * param_a.y + param_a.z);
        const float kjig = 1.0 / exp((param_a.x - uv.y) * param_a.y + param_a.z - 0.5);

        const float jiggle = param_a.w * kjig * rand(0.0, uv.y);
        const vec2 uvmod = vec2(uv.x - amount + jiggle, uv.y);

        if((uvmod.x < 0.0) || (uvmod.x > 1.0))
            target = vec4(0.0, 0.0, 0.0, 1.0);
        else target = texture(image, uvmod);

        target.yz *= 1.0 - 25.0 * kjig;
    }
    else {
        target = texture(image, uv);
    }
}
