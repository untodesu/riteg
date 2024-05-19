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

void main(void)
{
    const float samples = param_a.x;
    const float slope = param_a.y;
    const vec2 direction = param_a.zw;

    const float dist = 1.0 / samples;
    const vec2 texel = 1.0 / screen.xy;

    target = vec4(0.0, 0.0, 0.0, 0.0);

    const float X0 = -0.5 + param_b.x;
    const float X1 = +0.5 + param_b.x;
    for(float i = X0; i <= X1; i += dist) {
        const float coeff = exp(-1.0 * slope * abs(i - param_b.y));
        const vec2 uvmod = uv - i * texel * direction;

        if(uvmod.x >= 0.0 && uvmod.x <= 1.0 && uvmod.y >= 0.0 && uvmod.y <= 1.0) {
            /* All the samplers are set to CLAMP_TO_EDGE
             * but in this case we don't even want invalid
             * texture coordinates to contribute to the blur */
            target += coeff * texture(image, uv - i * texel * direction);
        }
    }

    target *= 1.0 + slope * exp(-1.0);
    target *= dist;
}
