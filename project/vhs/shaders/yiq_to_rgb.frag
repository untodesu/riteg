void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec4 color = texture(iChannel0, fragCoord / iResolution.xy);
    fragColor.r = dot(color.xyz, vec3(1.0000,  0.9469,  0.6236));
    fragColor.g = dot(color.xyz, vec3(1.0000, -0.2748, -0.6357));
    fragColor.b = dot(color.xyz, vec3(1.0000, -1.1000,  1.7000));
    fragColor.w = color.w;
}
