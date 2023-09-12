#version 450 core

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 target;

layout(binding = 0) uniform sampler2D image;

void main(void)
{
    const vec4 color = texture(image, uv);
    target.x = dot(color.xyz, vec3(0.2990,  0.5870,  0.1140));
    target.y = dot(color.xyz, vec3(0.5959, -0.2746, -0.3213));
    target.z = dot(color.xyz, vec3(0.2115, -0.5227,  0.3112));
    target.w = color.a;
}
