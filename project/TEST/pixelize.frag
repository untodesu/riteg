#if defined(RITEG)
#define BLOCK_WIDTH     iParam[0]
#define BLOCK_HEIGHT    iParam[1]
#define SIZE_RATIO_X    iParam[2]
#define SIZE_RATIO_Y    iParam[3]
#else
#define BLOCK_WIDTH     16.0
#define BLOCK_HEIGHT    16.0
#define SIZE_RATIO_X    0.5
#define SIZE_RATIO_Y    0.5
#endif

#define BACKGROUND vec4(0.0, 0.0, 0.0, 1.0)

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
