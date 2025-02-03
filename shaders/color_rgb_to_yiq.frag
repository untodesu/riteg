#version 450 core

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 target;

layout(binding = 0) uniform sampler2D image;

void main(void)
{
    const vec4 color = texture(image, uv);
    target.x = dot(color.xyz, vec3(0.3000,  0.5900,  0.1100));
    target.y = dot(color.xyz, vec3(0.5990, -0.2773, -0.3217));
    target.z = dot(color.xyz, vec3(0.2130, -0.5251,  0.3121));
    target.w = color.a;
}
