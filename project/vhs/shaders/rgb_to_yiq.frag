void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec4 color = texture(iChannel0, fragCoord / iResolution.xy);
    fragColor.x = dot(color.xyz, vec3(0.3000,  0.5900,  0.1100));
    fragColor.y = dot(color.xyz, vec3(0.5990, -0.2773, -0.3217));
    fragColor.z = dot(color.xyz, vec3(0.2130, -0.5251,  0.3121));
    fragColor.w = color.w;
}
