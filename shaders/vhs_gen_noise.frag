#version 450 core

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 target;

layout(binding = 0, std140) uniform uargs {
    vec4 screen;
    vec4 timing;
};

layout(binding = 1, std140) uniform uparams {
    vec4 param_a;
    vec4 param_b;
    vec4 param_c;
    vec4 param_d;
};

layout(binding = 0) uniform sampler2D specks;

float rand(float x, float y)
{
    return fract(sin(dot(vec3(x, y, timing.y), vec3(12.9898, 78.233, 37.719))) * 143758.5453);
}

void main(void)
{
    const float pixel = 1.0 / screen.x;
    const float fx = rand(uv.x, uv.y);
    const float fy = rand(fx, uv.x);
    const float fz = rand(fx, uv.y);
    const float steps = 4.0 + ceil(16.0 * fx);

    target = texture(specks, uv);

    for(float i = 0.1; i <= steps; ++i) {
        target += texture(specks, uv + vec2(i * pixel, 0.0)) / i * 16.0 * fy;
        target += texture(specks, uv - vec2(i * pixel, 0.0)) / i * 32.0 * fz;
    }

    target /= steps;
}
