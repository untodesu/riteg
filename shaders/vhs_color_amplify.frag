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

float nonlinear_amplify(float value, float saturate, float slope, float k)
{
    /* https://www.desmos.com/calculator/d5uwhnn5ln */
    const float clip = smoothstep(-saturate, saturate, value);
    const float cv = k * (slope * value + 2.0 * clip - 1.0) / (1.0 + slope);
    return cv;
}

void main(void)
{
    const vec4 color = texture(image, uv);

    target.x = color.x;
    target.w = color.w;

    // This simulates non-linear nature of amplifiers
    // implemented as transistors. I assume VCRs did use
    // either transistor-based amplifiers or something else
    // because high-frequency operational amplifiers were
    // and are expensive as shit, just a little bit too
    // expensive for a consumer grade format and players
    target.y = nonlinear_amplify(color.y, param_a.x, param_a.y, param_a.z);
    target.z = nonlinear_amplify(color.z, param_a.x, param_a.y, param_a.z);
}
