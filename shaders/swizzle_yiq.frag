#version 450 core

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 target;

layout(binding = 0) uniform sampler2D image_y;
layout(binding = 1) uniform sampler2D image_iq;

void main(void)
{
    const vec4 Y = texture(image_y, uv);
    const vec4 IQ = texture(image_iq, uv);
    target = vec4(Y.x, IQ.y, IQ.z, 1.0);
}
