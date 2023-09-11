#version 450 core

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 target;

layout(binding = 0) uniform sampler2D xws;
layout(binding = 1) uniform sampler2D yzs;

void main(void)
{
    const vec4 xw = texture(xws, uv);
    const vec4 yz = texture(yzs, uv);
    target.x = xw.x;
    target.y = yz.y;
    target.z = yz.z;
    target.w = xw.w;
}
