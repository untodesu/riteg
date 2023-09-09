#version 450 core

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 target;

layout(binding = 0) uniform sampler2D a;
layout(binding = 1) uniform sampler2D b;

void main(void)
{
    target = texture(a, uv) + texture(b, uv);
    target.x = clamp(target.x, 0.0, 1.0);
    target.y = clamp(target.y, 0.0, 1.0);
    target.z = clamp(target.z, 0.0, 1.0);
    target.w = clamp(target.w, 0.0, 1.0);
}
