#version 450 core

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 target;

layout(binding = 0) uniform sampler2D image;

void main(void)
{
    const vec4 color = texture(image, uv);
    target.r = dot(color.xyz, vec3(1.0000,  0.9469,  0.6236));
    target.g = dot(color.xyz, vec3(1.0000, -0.2748, -0.6357));
    target.b = dot(color.xyz, vec3(1.0000, -1.1000,  1.7000));
    target.a = color.w;
}
