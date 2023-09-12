#version 450 core

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 target;

layout(binding = 0) uniform sampler2D image;

void main(void)
{
    const vec4 color = texture(image, uv);
    target.r = dot(color.xyz, vec3(1.000,  0.956,  0.619));
    target.g = dot(color.xyz, vec3(1.000, -0.272, -0.647));
    target.b = dot(color.xyz, vec3(1.000, -1.106,  1.703));
    target.a = color.w;
}
