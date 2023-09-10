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

float rand(float x, float y)
{
    return fract(sin(dot(vec3(x, y, timing.y), vec3(12.9898, 78.233, 37.719))) * 143758.5453);
}

void main(void)
{
    const float pixel = 1.0 / screen.x;
    const float noise = rand(0.0, uv.y);

    if(uv.y * screen.y > param_a.x) {
        /* Small-scale UV noise without desaturation */
        target = texture(image, uv - vec2(param_a.y * pixel * noise, 0.0));
    }
    else {
        const vec4 color = texture(image, uv - vec2(param_a.z * pixel * noise, 0.0));
        const float luma = 0.299 * color.r + 0.587 * color.g + 0.114 * color.b;
        target = vec4(luma, luma, luma, color.a);
    }
}
