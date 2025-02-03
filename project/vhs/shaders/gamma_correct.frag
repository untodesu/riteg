#if RITEG
#define GAMMA iParam[0]
#else
#define GAMMA 2.2
#endif

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    float inv_gamma = 1.0 / GAMMA;
    vec4 color = texture(iChannel0, fragCoord / iResolution.xy);
    fragColor.x = pow(color.x, inv_gamma);
    fragColor.y = pow(color.y, inv_gamma);
    fragColor.z = pow(color.z, inv_gamma);
    fragColor.w = pow(color.w, inv_gamma);
}
