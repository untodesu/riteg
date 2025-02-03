#if RITEG
#define SX iParam[0]
#define SY iParam[1]
#else
#define SX 1.0
#define SY 1.0
#endif

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = fragCoord.xy / iResolution.xy;
    vec2 px = vec2(SX, SY) / iResolution.xy;
    fragColor = vec4(0.0, 0.0, 0.0, 0.0);
    fragColor -= texture(iChannel0, uv + vec2(px.x, 0.0));
    fragColor -= texture(iChannel0, uv + vec2(0.0, px.y));
    fragColor -= texture(iChannel0, uv - vec2(px.x, 0.0));
    fragColor -= texture(iChannel0, uv - vec2(0.0, px.x));
    fragColor += 5.0 * texture(iChannel0, uv);
}
