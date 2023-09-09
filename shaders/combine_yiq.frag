#version 450 core

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 target;

layout(binding = 0) uniform sampler2D image_y;
layout(binding = 1) uniform sampler2D image_iq;

void main(void)
{
    const vec4 y = texture(image_y, uv);
    const vec4 iq = texture(image_iq, uv);
    target.x = y.x;
    target.y = iq.y;
    target.z = iq.z;
    target.w = y.w;
}
