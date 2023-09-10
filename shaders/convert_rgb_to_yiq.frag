#version 450 core

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 target;

layout(binding = 0) uniform sampler2D image;

void main(void)
{
    const mat3x3 mtx = mat3x3(0.299, 0.5959, 0.2115, 0.587, -0.2746, -0.5227, 0.114, -0.3213, 0.3112);
    const vec4 color = texture(image, uv);
    target.xyz = mtx * color.rgb;
    target.w = color.a;
}
