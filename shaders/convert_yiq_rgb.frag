#version 450 core

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 target;

layout(binding = 0) uniform sampler2D image;

void main(void)
{
    const mat3x3 mtx = mat3x3(1.0, 1.0, 1.0, 0.956, -0.272, -1.106, 0.619, -0.647, 1.703);
    const vec4 color = texture(image, uv);
    target.xyz = mtx * color.xyz;
    target.w = color.a;
}
