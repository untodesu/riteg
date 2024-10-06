void mainImage(out vec4 target, in vec2 frag)
{
    vec2 uv = frag / iResolution.xy;
    vec4 vx = texture(iChannel0, uv);
    vec4 vy = texture(iChannel1, uv);
    target = 2.0 * vx - vy;
}
