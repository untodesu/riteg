#if RITEG
#define SATURATE    iParam[0]
#define SLOPE       iParam[1]
#define GAIN        iParam[2]
#else
#define SATURATE    0.75
#define SLOPE       0.10
#define GAIN        2.00
#endif

float nonlinear_amplify(float value, float saturate, float slope, float gain)
{
    /* https://www.desmos.com/calculator/d5uwhnn5ln */
    float clip = smoothstep(-saturate, saturate, value);
    float cv = gain * (slope * value + 2.0 * clip - 1.0) / (1.0 + slope);
    return cv;
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec4 color = texture(iChannel0, fragCoord / iResolution.xy);

    fragColor.x = color.x;
    fragColor.w = color.w;

    // This simulates non-linear nature of amplifiers
    // implemented as transistors. I assume VCRs did use
    // either transistor-based amplifiers or something else
    // because high-frequency operational amplifiers were
    // and are expensive as shit, just a little bit too
    // expensive for a consumer grade format and players
    fragColor.y = nonlinear_amplify(color.y, SATURATE, SLOPE, GAIN);
    fragColor.z = nonlinear_amplify(color.z, SATURATE, SLOPE, GAIN);
}
