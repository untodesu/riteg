// https://www.shadertoy.com/view/4cf3W8

#define BLOCK_WIDTH 8.0
#define BLOCK_HEIGHT 8.0
#define SIZE_RATIO_X 0.50
#define SIZE_RATIO_Y 0.50
#define BACKGROUND vec4(0.0, 0.0, 0.0, 0.0);

void mainImage(out vec4 target, in vec2 pixcoord)
{
    vec2 pixel = 1.0 / iResolution.xy; 
    vec2 texcoord = vec2(0.0, 0.0);
    texcoord.x = BLOCK_WIDTH * pixel.x * floor(pixcoord.x / BLOCK_WIDTH);
    texcoord.y = BLOCK_HEIGHT * pixel.y * floor(pixcoord.y / BLOCK_HEIGHT);

    float gap_width = BLOCK_WIDTH * (1.0 - SIZE_RATIO_X);
    float gap_height = BLOCK_HEIGHT * (1.0 - SIZE_RATIO_Y);

    float mod_x = mod(pixcoord.x, BLOCK_WIDTH);
    float mod_y = mod(pixcoord.y, BLOCK_HEIGHT);

    if((mod_x <= gap_width) || (mod_y <= gap_height)) {
        // This is the gap between colored blocks
        target = BACKGROUND;
    }
    else {
        target = vec4(0.0, 0.0, 0.0, 0.0);
        for(float i = 0.0; i < BLOCK_WIDTH; ++i)
            for(float j = 0.0; j < BLOCK_HEIGHT; ++j)
                target += texture(iChannel0, texcoord + pixel * vec2(i, j));
        target /= BLOCK_WIDTH * BLOCK_HEIGHT;
    }
}
