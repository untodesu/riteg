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

layout(binding = 0) uniform sampler2D image;
layout(binding = 1) uniform sampler2D specks;

float rand(float x, float y)
{
    return fract(sin(dot(vec3(x, y, timing.y), vec3(12.9898, 78.233, 37.719))) * 143758.5453);
}

void main(void)
{
    const float fx = rand(uv.x, uv.y);
    const float fy = rand(fx, uv.x);
    const float fz = rand(fx, uv.y);
    const float fd = rand(0.0, uv.y);
    const float pixel = 1.0 / screen.x;
    const float isteps = 2.0 + (1.0 * fx);
    const float nsteps = 4.0 + (16.0 * fx);

    vec3 image_color = vec3(0.0, 0.0, 0.0);
    vec3 specks_color = vec3(0.0, 0.0, 0.0);

    for(float i = 0.1; i <= isteps; ++i)
        image_color += texture(image, uv - vec2((i + fd * param_a.x) * pixel, 0.0)).xyz;
    image_color /= isteps;

    for(float i = 0.1; i <= nsteps; ++i)
        specks_color += texture(specks, uv + vec2(i * pixel, 0.0)).xyz / i * 16.0 * fy;
    for(float i = 0.1; i <= nsteps; ++i)
        specks_color += texture(specks, uv - vec2(i * pixel, 0.0)).xyz / i * 32.0 * fz;
    specks_color /= nsteps * 2.0;

    target.x = mix(image_color.x, 1.0, clamp(0.5 * specks_color.x, 0.0, 1.0));
    target.y = mix(image_color.y, 1.0, clamp(0.5 * specks_color.y, 0.0, 1.0));
    target.z = mix(image_color.z, 1.0, clamp(0.5 * specks_color.z, 0.0, 1.0));
    target.w = 1.0;
}
