#version 450 core

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 target;

layout(binding = 0) uniform sampler2D image;
layout(binding = 1) uniform sampler2D dropout;

void main(void)
{
    const vec4 color = texture(image, uv);
    const vec4 noise = texture(dropout, uv);

    target.x = mix(color.x, 1.0, noise.x);
    target.y = mix(color.y, noise.y, noise.x);
    target.z = mix(color.z, noise.z, noise.x);
    target.w = color.w;
}
