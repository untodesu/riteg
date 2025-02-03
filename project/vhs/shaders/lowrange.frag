#if RITEG
#define LR_AMP iParam[0]
#define LR_FRQ iParam[1]
#else
#define LR_AMP 0.400
#define LR_FRQ 0.750
#endif

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec4 yiq = texture(iChannel0, fragCoord / iResolution.xy);
    fragColor.x = yiq.x;
    fragColor.y = 2.0 * LR_AMP * atan(10.0 * LR_FRQ * yiq.y) / 3.14159265359;
    fragColor.z = 2.0 * LR_AMP * atan(10.0 * LR_FRQ * yiq.z) / 3.14159265359;
    fragColor.w = yiq.w;
}
