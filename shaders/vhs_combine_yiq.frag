#version 450 core

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 target;

layout(binding = 0) uniform sampler2D image_luma;
layout(binding = 1) uniform sampler2D image_chroma;

void main(void)
{
    const vec4 luma = texture(image_luma, uv);
    const vec4 chroma = texture(image_chroma, uv);
    target.x = luma.x;
    target.y = chroma.y;
    target.z = chroma.z;
    target.w = luma.w;
}
