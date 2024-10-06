void mainImage(out vec4 target, in vec2 frag)
{
    /* https://github.com/Jam3/glsl-fast-gaussian-blur */
    const vec2 direction = vec2(3.5, 0.0);
    vec2 uv = frag / iResolution.xy;
    vec2 off1 = 1.411764705882353 * direction / iResolution.xy;
    vec2 off2 = 3.294117647058823 * direction / iResolution.xy;
    vec2 off3 = 5.176470588235294 * direction / iResolution.xy;
    target = texture(iChannel0, uv) * 0.1964825501511404;
    target += texture(iChannel0, uv + off1) * 0.2969069646728344;
    target += texture(iChannel0, uv - off1) * 0.2969069646728344;
    target += texture(iChannel0, uv + off2) * 0.09447039785044732;
    target += texture(iChannel0, uv - off2) * 0.09447039785044732;
    target += texture(iChannel0, uv + off3) * 0.010381362401148057;
    target += texture(iChannel0, uv - off3) * 0.010381362401148057;
}
