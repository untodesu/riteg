#if RITEG
#define DIR_X iParam[0]
#define DIR_Y iParam[1]
#else
#define DIR_X 1.0
#define DIR_Y 1.0
#endif

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = fragCoord.xy / iResolution.xy;
    vec2 off1 = 1.411764705882353 * vec2(DIR_X, DIR_Y) / iResolution.xy;
    vec2 off2 = 3.294117647058823 * vec2(DIR_X, DIR_Y) / iResolution.xy;
    vec2 off3 = 5.176470588235294 * vec2(DIR_X, DIR_Y) / iResolution.xy;
    fragColor = texture(iChannel0, uv) * 0.1964825501511404;
    fragColor += texture(iChannel0, uv + off1) * 0.2969069646728344;
    fragColor += texture(iChannel0, uv - off1) * 0.2969069646728344;
    fragColor += texture(iChannel0, uv + off2) * 0.09447039785044732;
    fragColor += texture(iChannel0, uv - off2) * 0.09447039785044732;
    fragColor += texture(iChannel0, uv + off3) * 0.010381362401148057;
    fragColor += texture(iChannel0, uv - off3) * 0.010381362401148057;
}
