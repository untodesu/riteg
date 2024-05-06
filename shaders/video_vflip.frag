#version 450 core

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 target;

layout(binding = 0) uniform sampler2D image;

void main(void)
{
    const vec2 uvmod = vec2(uv.x, 1.0 - uv.y);
    target = texture(image, uvmod);
}
