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
    vec2 off = 1.33333333 * vec2(DIR_X, DIR_Y) / iResolution.xy;
    vec2 off2 = 3.2307692308 * vec2(DIR_X, DIR_Y) / iResolution.xy;
    fragColor = texture(iChannel0, uv) * 0.29411764705882354;
    fragColor += texture(iChannel0, uv + off) * 0.35294117647058826;
    fragColor += texture(iChannel0, uv - off) * 0.35294117647058826;
}
