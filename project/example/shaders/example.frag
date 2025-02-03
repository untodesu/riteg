// This shader is directly copypasted from a new shader
// page on Shadertoy: https://www.shadertoy.com/new

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = fragCoord / iResolution.xy;

    // Time varying pixel color
    vec3 col = 0.5 + 0.5 * cos(iTime + uv.xyx + vec3(0.0, 2.0, 4.0));

    // Output to screen
    fragColor = vec4(col, 1.0);
}
