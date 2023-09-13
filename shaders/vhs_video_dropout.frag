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
    const vec2 huv = 0.5 * uv;
    const float fx = rand(huv.x, huv.y);
    const float fy = rand(fx, huv.y);
    const float fz = rand(fx, huv.y);
    const float pixel = 1.0 / screen.x;
    const float steps = 4.0 + ceil(16.0 * fx);
    const float thres = min(param_a.x, 1.0 - param_a.y * pow(sin(3.14159265359 * pow(1 - uv.y - param_a.z, 2.0 * param_a.w)), 2.0));

    float noise = step(thres, rand(huv.x, huv.y));
    for(float i = 1.0; i <= steps; ++i)
        noise += step(thres, rand(huv.x + i * pixel, huv.y)) / i * 16.0 * fy;
    for(float i = 1.0; i <= steps; ++i)
        noise += step(thres, rand(huv.x - i * pixel, huv.y)) / i * 32.0 * fz;
    noise /= 2.0 * steps;

    const vec4 luma = texture(image, uv + vec2(15.0 * noise * (1.0 - thres), 0.0));
    const vec4 chroma = texture(image, uv);
    target.x = mix(luma.x, 1.0, noise);
    target.y = chroma.y;
    target.z = chroma.z;
    target.w = chroma.w;
}
