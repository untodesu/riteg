// This shader is directly copypasted from my Shadertoy
// account: https://www.shadertoy.com/view/4cf3W8

// Because RITEG doesn't feature dynamic shader recompilation
// following changes in either pipeline or shader source code
// it makes sense to provide an extended uniform with parameters;
// The uniform is defined as:   uniform float iParam[16]
// The uniform is only defined in an environment with RITEG
// macro defined and set to a non-zero value, so wrapping constant
// definitions around an ifdef block should make the shader work
// out of the box in both RITEG and Shadertoy environments

#ifdef RITEG
#define BLOCK_WIDTH     iParam[0]
#define BLOCK_HEIGHT    iParam[1]
#define SIZE_RATIO_X    iParam[2]
#define SIZE_RATIO_Y    iParam[3]
#else
#define BLOCK_WIDTH     8.0
#define BLOCK_HEIGHT    8.0
#define SIZE_RATIO_X    0.5
#define SIZE_RATIO_Y    0.5
#endif

#define BACKGROUND vec4(0.0, 0.0, 0.0, 0.0)

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 pixel = 1.0 / iResolution.xy;

    vec2 uv = vec2(0.0, 0.0);
    uv.x = BLOCK_WIDTH * pixel.x * floor(fragCoord.x / BLOCK_WIDTH);
    uv.y = BLOCK_HEIGHT * pixel.y * floor(fragCoord.y / BLOCK_HEIGHT);

    float gap_width = BLOCK_WIDTH * (1.0 - SIZE_RATIO_X);
    float gap_height = BLOCK_HEIGHT * (1.0 - SIZE_RATIO_Y);
    
    float mod_x = mod(fragCoord.x, BLOCK_WIDTH);
    float mod_y = mod(fragCoord.y, BLOCK_HEIGHT);

    vec4 color = vec4(0.0, 0.0, 0.0, 0.0);
    for(float i = 0.0; i < BLOCK_WIDTH; i += 1.0)
    for(float j = 0.0; j < BLOCK_HEIGHT; j += 1.0)
        color += texture(iChannel0, uv + pixel * vec2(i, j));
    color /= BLOCK_WIDTH * BLOCK_HEIGHT;

    if((mod_x <= gap_width) || (mod_y <= gap_height))
        fragColor = BACKGROUND;
    else fragColor = color;
}
