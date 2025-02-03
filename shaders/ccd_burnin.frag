#version 450 core

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 target;

layout(binding = 0, std140) uniform params {
    vec4 param_a;
    vec4 param_b;
    vec4 param_c;
    vec4 param_d;
    vec4 screen;
    vec4 timing;
};

layout(binding = 0) uniform sampler2D image;
layout(binding = 1) uniform sampler2D history;

void main(void)
{
    const vec4 color = texture(image, uv);
    const vec4 hcolor = param_a.y * texture(history, uv);
    
    if(color.x >= param_a.x)
        target = hcolor + color;
    else target = hcolor;
}
