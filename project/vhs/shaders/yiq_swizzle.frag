void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = fragCoord.xy / iResolution.xy;
    vec4 YW = texture(iChannel0, uv);
    vec4 IQ = texture(iChannel1, uv);
    fragColor.x = YW.x;
    fragColor.y = IQ.y;
    fragColor.z = IQ.z;
    fragColor.w = YW.w;
}
