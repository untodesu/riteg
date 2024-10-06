void mainImage(out vec4 target, in vec2 frag)
{
    /* Backported straight from RITEG */
    vec4 color = texture(iChannel0, frag / iResolution.xy);
    float y = dot(color.rgb, vec3(0.2990, 0.5870, 0.1140));
    target = vec4(y, y, y, color.a);
}
