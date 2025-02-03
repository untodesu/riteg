#if RITEG
#define AMP1 iParam[0]
#define AMP2 iParam[1]
#else
#define AMP1 1.0
#define AMP2 1.0
#endif

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = fragCoord.xy / iResolution.xy;
    vec4 color1 = texture(iChannel0, uv);
    vec4 color2 = texture(iChannel1, uv);
    fragColor = AMP1 * color1 - AMP2 * color2;
}
