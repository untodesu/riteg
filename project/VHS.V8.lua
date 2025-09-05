-- With a new iteration of RITEG's shader API, a new iteration
-- of the VHS filter comes. This time RITEG.V2b features a complete
-- rewrite of the pipeline with less shader passes and more flexible
-- configuration thanks to the RITEG.V2b running as a Lua runtime

-- This time the pipeline relies more on existing shaders
-- that are just ported to better suit the purpose of a multi-pass VHS filter

-- ATTRIBUTION ATTRIBUTION ATTRIBUTION ATTRIBUTION
-- ATTRIBUTION ATTRIBUTION ATTRIBUTION ATTRIBUTION
-- ATTRIBUTION ATTRIBUTION ATTRIBUTION ATTRIBUTION
-- ATTRIBUTION ATTRIBUTION ATTRIBUTION ATTRIBUTION
--  VHS Compression: https://www.shadertoy.com/view/tsfXWj
--  NTSC Filter: https://www.shadertoy.com/view/wlScWG

print("VHS.V8 filter by untodesu")
print("Based on mostly own work from 2022 with the following bits:")
print("- VHS compression: https://www.shadertoy.com/view/tsfXWj by mpalko")
print("- NTSC filter: https://www.shadertoy.com/view/wlScWG by Hatchling")

-- Specify base resolution at which the filter
-- actually operates. This has nothing to do with
-- the output resolution, although output defaults to base
local base_wide <const> = 640
local base_tall <const> = 480

-- Specify the output resolution of the filter
local out_wide = math.min(math.max(riteg.get_option_number("output-width", base_wide), 320), 2560)
local out_tall = math.min(math.max(riteg.get_option_number("output-height", base_tall), 240), 1920)

-- Ensure we set up the random generator with a unique
-- seed every time so the generational loss does not just
-- overlay; I had an idea to use the seed in shaders but didn't
-- read into lua documentation enough to find out one has to initialize
-- the RNG before calling math.random [untodesu, 2025-09-05]
math.randomseed(os.time())

-- A bunch of GLSL code that is shared between all the
-- passes in the filter pipeline; this contains bits from
-- both VHS compression, NTSC filter and my own code
local common_glsl <const> = string.format([[
    const float PI = 3.14159265359;

    const uint RNG_SEED = %dU;

    // Increasing this value increases ringing artifacts.
    // Careful, higher values are expensive.
    const int DECODE_WINDOW_WIDTH = 16;
    const int ENCODE_WINDOW_WIDTH = 4;

    // Simulated AM signal transmission.
    const float AM_CARRIERSIGNAL_WAVELENGTH = 2.0;
    const float AM_DECODE_HIGHPASS_WAVELENGTH = 2.0;
    const float AM_DEMODULATE_WAVELENGTH = 2.0;

    // Wavelength of the color signal. 
    const float COLORBURST_WAVELENGTH_ENCODER = 3.0;
    const float COLORBURST_WAVELENGTH_DECODER = 3.0;

    // Lowpassing of luminance before encoding.
    // If this value is less than the colorburst wavelength,
    // luminance values will be interpreted as chrominance,
    // resulting in color fringes near edges.
    const float YLOWPASS_WAVELENGTH = 1.0;

    // The higher these values are, the more smeary colors will be.
    const float ILOWPASS_WAVELENGTH = 8.0;
    const float QLOWPASS_WAVELENGTH = 11.0;

    // The higher this value, the blurrier the image.
    // If this value is less than or equal to the colorburst wavelength,
    // you'll have striping artifacts.
    const float DECODE_LOWPASS_WAVELENGTH = 4.5;

    const float NTSC_SCALE = 1.0; // Change the overall scale of the NTSC-style encoding and decoding artifacts
    const float PHASE_ALTERNATION = PI; // PI for PAL-like
    const float NOISE_STRENGTH = 0.015625; // Amount of TV static
    const float SATURATION = 3.0; // Saturation control
    const float WINDOW_BIAS = 0.0; // Offsets shape of window. This can make artifacts smear to one side or the other.

    const float VHS_CUTOFF = 0.01;
    const float VHS_DROPOUT_STRENGTH = 0.000001;
    const int VHS_DROPOUT_BLUR_STEPS = 64;

    const vec2 VHS_MAXRES_Y = vec2(333.0, 480.0);
    const vec2 VHS_MAXRES_IQ = vec2(40.0, 480.0);
    const vec2 VHS_BLUR_AMT = vec2(0.2, 0.2);

    const float GAMMA_CORRECTION = 1.0;

    const mat3 RGB_TO_YIQ = mat3(0.299, 0.587, 0.114, 0.595, -0.274, -0.3213, 0.2115,-0.5227, 0.3112);
    const mat3 YIQ_TO_RGB = mat3(1.0, 0.956, 0.619, 1.0, -0.272, -0.647, 1.0, -1.106, 1.703);
    
    uint Common_newRngStateEx(vec2 uv, uint entropy)
    {
        return uint(uint(uv.x) * 1973U + uint(uv.y) * 9277U + uint(iFrame) * 26699U + entropy * 34961U + RNG_SEED * 41077U) | 1U;
    }

    uint Common_newRngState(vec2 uv)
    {
        return Common_newRngStateEx(uv, 0U);
    }

    // https://www.shadertoy.com/view/wtSyWm
    uint Common_wangHash(inout uint seed)
    {
        seed = uint(seed ^ uint(61)) ^ uint(seed >> uint(16));
        seed *= uint(9);
        seed = seed ^ (seed >> 4);
        seed *= uint(0x27d4eb2d);
        seed = seed ^ (seed >> 15);
        return seed;
    }

    float Common_randomFloat(inout uint state)
    {
        return float(Common_wangHash(state)) / 4294967296.0;
    }
    
    float NTSC_sinc(float x)
    {
        if(x == 0.0)
            return 1.0;
        return sin(PI * x) / x / PI;
    }

    float NTSC_cosineWindow(float x)
    {
        x = atanh(x);
        x += WINDOW_BIAS;
        x = tanh(x);
        return 0.5 + 0.5 * cos(PI * x);
    }

    float NTSC_fullEncode(sampler2D sampler, in vec2 uv, in float pixelWidth, bool alternatePhase)
    {
        vec3 yiq = vec3(0.0, 0.0, 0.0);

        for(int i = -ENCODE_WINDOW_WIDTH; i <= ENCODE_WINDOW_WIDTH; ++i) {
            float window = NTSC_cosineWindow(float(i) / float(ENCODE_WINDOW_WIDTH + 1)); 
            float sincY = NTSC_sinc(float(i) / YLOWPASS_WAVELENGTH) / YLOWPASS_WAVELENGTH;
            float sincI = NTSC_sinc(float(i) / ILOWPASS_WAVELENGTH) / ILOWPASS_WAVELENGTH;
            float sincQ = NTSC_sinc(float(i) / QLOWPASS_WAVELENGTH) / QLOWPASS_WAVELENGTH;
            
            vec2 uvWithOffset = vec2(uv.x + float(i) * pixelWidth, uv.y);
            vec3 yiqSample = RGB_TO_YIQ * clamp(texture(sampler, uvWithOffset).rgb, vec3(0.0), vec3(1.0));

            yiq.x += yiqSample.x * sincY * window;
            yiq.y += yiqSample.y * sincI * window;
            yiq.z += yiqSample.z * sincQ * window;
        }

        float phase = uv.x * PI / (COLORBURST_WAVELENGTH_ENCODER * pixelWidth);
        if(alternatePhase)
            phase += PHASE_ALTERNATION;
        float phaseAM = uv.x * PI / (AM_CARRIERSIGNAL_WAVELENGTH * pixelWidth);
        return sin(phaseAM) * (yiq.x + yiq.y * sin(phase) + yiq.z * cos(phase));
    }

    float NTSC_decodeAM(sampler2D sampler, in vec2 uv, in float pixelWidth)
    {
        float phaseAM = uv.x * PI / (AM_DEMODULATE_WAVELENGTH * pixelWidth);        
        float windowWeight = 0.0;
        float decoded = 0.0;

        for(int i = -DECODE_WINDOW_WIDTH; i <= DECODE_WINDOW_WIDTH; ++i) {
            float window = NTSC_cosineWindow(float(i) / float(DECODE_WINDOW_WIDTH+1));

            vec2 uvWithOffset = vec2(uv.x + float(i) * pixelWidth, uv.y);

            float sinc = NTSC_sinc(float(i) / AM_DECODE_HIGHPASS_WAVELENGTH) / AM_DECODE_HIGHPASS_WAVELENGTH;
            float encodedSample = texture(sampler, uvWithOffset).x;
            
            decoded += encodedSample * sinc * window;
            windowWeight += window;
        }
        
        return decoded * sin(phaseAM) * 4.0;
    }

    vec3 NTSC_fullDecodeYIQ(sampler2D sampler, in vec2 uv, in float pixelWidth, ivec2 rng, bool alternatePhase)
    {
        uint seed = uint(rng.y);
        vec2 originalUV = uv;
        
        vec3 rowNoiseIntensity = vec3(Common_randomFloat(seed), Common_randomFloat(seed), Common_randomFloat(seed));
        rowNoiseIntensity = pow(rowNoiseIntensity, vec3(500.0)) * 0.125;
        
        float phaseNoise = Common_randomFloat(seed) * 2.0 - 1.0;
        phaseNoise *= rowNoiseIntensity.y * 0.5 * 3.1415927 * NOISE_STRENGTH;

        float frequencyNoise = Common_randomFloat(seed) * 2.0 - 1.0;
        frequencyNoise *= rowNoiseIntensity.z * 0.5 * 3.1415927 * NOISE_STRENGTH;
        
        vec3 yiq = vec3(0);
        float alt = alternatePhase ? PHASE_ALTERNATION : 0.0;
        float windowWeight = 0.0;

        for(int i = -DECODE_WINDOW_WIDTH; i <= DECODE_WINDOW_WIDTH; ++i) {
            float window = NTSC_cosineWindow(float(i) / float(DECODE_WINDOW_WIDTH+1)); 
            vec2 uvWithOffset = vec2(uv.x + float(i) * pixelWidth, uv.y);
            vec2 originalUVWithOffset = vec2(originalUV.x + float(i) * pixelWidth, originalUV.y);
            float phase = originalUVWithOffset.x * PI / ((COLORBURST_WAVELENGTH_DECODER + frequencyNoise) * pixelWidth) + phaseNoise + alt;
            
            float sincY = NTSC_sinc(float(i) / DECODE_LOWPASS_WAVELENGTH) / DECODE_LOWPASS_WAVELENGTH;
            float sinI = sin(phase);
            float sinQ = cos(phase);
            
            float encodedSample = texture(sampler, uvWithOffset).x;
            yiq.x += encodedSample * sincY * window;
            yiq.y += encodedSample * sinI * window;
            yiq.z += encodedSample * sinQ * window;
            windowWeight += window;
        }
        
        yiq.yz *= SATURATION / windowWeight;

        return yiq;
    }
    
    vec3 NTSC_fullDecodeRGB(sampler2D sampler, in vec2 uv, in float pixelWidth, ivec2 rng, bool alternatePhase)
    {
        return max(vec3(0.0, 0.0, 0.0), YIQ_TO_RGB * NTSC_fullDecodeYIQ(sampler, uv, pixelWidth, rng, alternatePhase));
    }

    vec4 VHS_cubic(float v)
    {
        vec4 n = vec4(1.0, 2.0, 3.0, 4.0) - v;
        vec4 s = n * n * n;
        float x = s.x;
        float y = s.y - 4.0 * s.x;
        float z = s.z - 4.0 * s.y + 6.0 * s.x;
        float w = 6.0 - x - y - z;
        return vec4(x, y, z, w) * (1.0/6.0);
    }

    vec4 VHS_textureBicubic(sampler2D sampler, vec2 texCoords)
    {
        vec2 texSize = vec2(textureSize(sampler, 0));
        vec2 invTexSize = vec2(1.0) / texSize;

        texCoords = texCoords * texSize - 0.5;

        vec2 fxy = fract(texCoords);
        texCoords -= fxy;

        vec4 xcubic = VHS_cubic(fxy.x);
        vec4 ycubic = VHS_cubic(fxy.y);

        vec4 c = texCoords.xxyy + vec2 (-0.5, +1.5).xyxy;

        vec4 s = vec4(xcubic.xz + xcubic.yw, ycubic.xz + ycubic.yw);
        vec4 offset = c + vec4 (xcubic.yw, ycubic.yw) / s;

        offset *= invTexSize.xxyy;

        vec4 sample0 = texture(sampler, offset.xz);
        vec4 sample1 = texture(sampler, offset.yz);
        vec4 sample2 = texture(sampler, offset.xw);
        vec4 sample3 = texture(sampler, offset.yw);

        float sx = s.x / (s.x + s.y);
        float sy = s.z / (s.z + s.w);

        return mix(mix(sample3, sample2, sx), mix(sample1, sample0, sx), sy);
    }
]], math.floor(math.random(0, 65535)))

local function lowerpass_shader(dx, dy)
    return common_glsl .. string.format([[
        #define DIR_X %f
        #define DIR_Y %f

        void mainImage(out vec4 fragColor, in vec2 fragCoord)
        {
            vec2 uv = fragCoord.xy / iResolution.xy;
            vec2 off1 = 1.411764705882353 * vec2(DIR_X, DIR_Y) / iResolution.xy;
            vec2 off2 = 3.294117647058823 * vec2(DIR_X, DIR_Y) / iResolution.xy;
            vec2 off3 = 5.176470588235294 * vec2(DIR_X, DIR_Y) / iResolution.xy;
            fragColor = texture(iChannel0, uv) * 0.1964825501511404;
            fragColor += texture(iChannel0, uv + off1) * 0.2969069646728344;
            fragColor += texture(iChannel0, uv - off1) * 0.2969069646728344;
            fragColor += texture(iChannel0, uv + off2) * 0.09447039785044732;
            fragColor += texture(iChannel0, uv - off2) * 0.09447039785044732;
            fragColor += texture(iChannel0, uv + off3) * 0.010381362401148057;
            fragColor += texture(iChannel0, uv - off3) * 0.010381362401148057;
        }
    ]], dx, dy)
end

local yiq_to_rgb_src <const> = common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord / iResolution.xy;
        vec3 yiq = texture(iChannel0, uv).xyz;
        fragColor.rgb = YIQ_TO_RGB * yiq;
        fragColor.w = 1.0;
    }
]]

local function ntsc_encode_src(step)
    return common_glsl .. string.format([[
        void mainImage(out vec4 fragColor, in vec2 fragCoord)
        {
            uint rngState = Common_newRngStateEx(fragCoord, %dU);
            uint rngStateRow = Common_newRngStateEx(vec2(0.0, fragCoord.y), %dU);
            
            vec2 uv = fragCoord / iResolution.xy;
            vec2 pixelSize = 1.0 / iResolution.xy;
            
            float encoded = NTSC_fullEncode(iChannel0, uv, pixelSize.x * NTSC_SCALE, int(fragCoord.y) %% 2 == 0);
            
            float snowNoise = Common_randomFloat(rngState) - 0.5;
            float sineNoise = sin(uv.x * 200.0 + uv.y * -50.0 + fract(iTime * iTime) * 3.1415927 * 2.0) * 0.065;
            float saltPepperNoise = Common_randomFloat(rngState) * 2.0 - 1.0;
    
            saltPepperNoise = sign(saltPepperNoise) * pow(abs(saltPepperNoise), 200.0) * 10.0;
        
            float rowNoise = Common_randomFloat(rngStateRow) * 2.0 - 1.0;
            rowNoise *= 0.1;   
        
            float rowSaltPepper = Common_randomFloat(rngStateRow) * 2.0 - 1.0;
            rowSaltPepper = sign(rowSaltPepper) * pow(abs(rowSaltPepper), 200.0) * 1.0;
            
            encoded += (snowNoise + saltPepperNoise + sineNoise + rowNoise + rowSaltPepper) * NOISE_STRENGTH; 
            
            fragColor.xyz = vec3(encoded);
            fragColor.w = 1.0;
        }
    ]], step, step)
end

local ntsc_decode_y_src <const> = common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord / iResolution.xy;
        vec2 pixelSize = 1.0 / iResolution.xy;
        
        float value = NTSC_decodeAM(iChannel0, uv, pixelSize.x);
        
        fragColor.xyz = vec3(value);
        fragColor.w = 1.0;
    }
]]

local ntsc_decode_iq_src <const> = common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord / iResolution.xy;
        vec2 pixelSize = 1.0 / iResolution.xy;
        
        uint rngStateRow = Common_newRngState(vec2(0.0, fragCoord.y));
        uint rngStateCol = Common_newRngState(vec2(fragCoord.x, 0.0));

        fragColor.xyz = NTSC_fullDecodeYIQ(iChannel0, uv, pixelSize.x * NTSC_SCALE, ivec2(rngStateCol, rngStateRow), int(fragCoord.y) % 2 == 0);
        fragColor.w = 1.0;
    }
]]

local gamma_correct = riteg.create_shader(base_wide, base_tall, common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord / iResolution.xy;
        vec4 color = texture(iChannel0, uv);
        fragColor.rgb = pow(color.rgb, vec3(1.0 / GAMMA_CORRECTION));
        fragColor.a = color.a;
    }
]], { iChannel0 = riteg.get_image_source() })

local ntsc_pre_vcr_encode = riteg.create_shader(2.0 * base_wide, base_tall, ntsc_encode_src(0), { iChannel0 = gamma_correct })
local ntsc_pre_vcr_decode_y = riteg.create_shader(2.0 * base_wide, base_tall, ntsc_decode_y_src, { iChannel0 = ntsc_pre_vcr_encode })
local ntsc_pre_vcr_decode_iq = riteg.create_shader(2.0 * base_wide, base_tall, ntsc_decode_iq_src, { iChannel0 = ntsc_pre_vcr_decode_y })

local vhs_cutoff = riteg.create_shader(base_wide, base_tall, common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord.xy / iResolution.xy;
        vec4 color = texture(iChannel0, uv);
        fragColor = color * step(VHS_CUTOFF, 1.0 - uv.x);
    }
]], { iChannel0 = ntsc_pre_vcr_decode_iq })

local vhs_downsample = riteg.create_shader(base_wide, base_tall, common_glsl .. [[
    vec3 downsampleVideo(vec2 uv, vec2 pixelSize, ivec2 samples)
    {
        vec2 uvStart = uv - pixelSize / 2.0;
        vec2 uvEnd = uv + pixelSize;        
        vec3 result = vec3(0.0, 0.0, 0.0);

        for (int i_u = 0; i_u < samples.x; ++i_u) {
            float u = mix(uvStart.x, uvEnd.x, float(i_u) / float(samples.x));

            for (int i_v = 0; i_v < samples.y; ++i_v) {
                float v = mix(uvStart.y, uvEnd.y, float(i_v) / float(samples.y));

                result += texture(iChannel0, vec2(u, v)).rgb;
            }
        }    
        
        return result / float(samples.x * samples.y);
    }

    vec3 downsampleVideo(vec2 fragCoord, vec2 downsampledRes)
    {
        if (fragCoord.x > downsampledRes.x || fragCoord.y > downsampledRes.y) {
            return vec3(0.0);
        }
        
        vec2 uv = fragCoord / downsampledRes;
        vec2 pixelSize = 1.0 / downsampledRes;
        ivec2 samples = ivec2(8, 1);
        
        pixelSize *= 1.0 + VHS_BLUR_AMT; // Slight box blur to avoid aliasing
        
        return downsampleVideo(uv, pixelSize, samples);
    }
    
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 resLuminance = min(VHS_MAXRES_Y, vec2(iResolution));
        vec2 resChroma = min(VHS_MAXRES_IQ, vec2(iResolution));    
        
        float luminance = downsampleVideo(fragCoord, resLuminance).x;
        vec2 chroma = downsampleVideo(fragCoord, resChroma).yz;
        
        fragColor = vec4(luminance, chroma, 1.0);
    }
]], { iChannel0 = vhs_cutoff })

local vhs_upsample = riteg.create_shader(base_wide, base_tall, common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord / iResolution.xy;
        
        vec2 resLuminance = min(VHS_MAXRES_Y, vec2(iResolution));
        vec2 resChroma = min(VHS_MAXRES_IQ, vec2(iResolution));
        
        vec2 uvLuminance = uv * (resLuminance / vec2(iResolution));
        vec2 uvChroma = uv * (resChroma / vec2(iResolution));
        
        float luminance = VHS_textureBicubic(iChannel1, uvLuminance).x;
        vec2 chroma = VHS_textureBicubic(iChannel1, uvChroma).yz;
        
        fragColor = vec4(luminance, chroma, 1.0);
    }
]], { iChannel0 = vhs_downsample })

local vhs_lowerpass = riteg.create_shader(base_wide, base_tall, lowerpass_shader(1.0, 0.0), { iChannel0 = vhs_upsample })

local vhs_upsample_combined = riteg.create_shader(base_wide, base_tall, common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord / iResolution.xy;
        vec4 vhs_upsample = texture(iChannel0, uv);
        vec4 vhs_lowerpass = texture(iChannel1, uv);
        
        fragColor.xyz = 3.0 * vhs_upsample.xyz - 2.0 * vhs_lowerpass.xyz;
        fragColor.w = 1.0;
    }
]], { iChannel0 = vhs_upsample, iChannel1 = vhs_lowerpass })

local vhs_sharpen = riteg.create_shader(base_wide, base_tall, common_glsl .. [[
    #define SHARPEN_AMOUNT 1.0

    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord.xy / iResolution.xy;
        vec4 color = texture(iChannel0, uv);
        color += SHARPEN_AMOUNT * dFdx(color);
        color += SHARPEN_AMOUNT * dFdy(color);
        fragColor = color;
    }
]], { iChannel0 = vhs_upsample_combined })

local vhs_dropout = riteg.create_shader(base_wide / 2, base_tall, common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        uint rngState = Common_newRngState(fragCoord);

        float dropout = 2.0 * Common_randomFloat(rngState) - 1.0;

        uint rngStateI = Common_newRngState(vec2(dropout, fragCoord.x));
        uint rngStateQ = Common_newRngState(vec2(dropout, fragCoord.y));

        fragColor.x = step(1.0 - clamp(VHS_DROPOUT_STRENGTH, 0.0, 1.0), dropout);
        fragColor.y = 2.0 * Common_randomFloat(rngStateI) - 1.0;
        fragColor.z = 2.0 * Common_randomFloat(rngStateQ) - 1.0;
        fragColor.w = 1.0;
    }
]])

local vhs_dropout_blur = riteg.create_shader(base_wide, base_tall, common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        uint rngState = Common_newRngState(fragCoord);

        vec2 uv = fragCoord.xy / iResolution.xy;
        vec2 pixel = 1.0 / iResolution.xy;

        fragColor = vec4(0.0, 0.0, 0.0, 1.0);

        int steps = int(VHS_DROPOUT_BLUR_STEPS * Common_randomFloat(rngState));

        for(int i = 0; i < steps; ++i) {
            vec2 uvmod = vec2(uv.x - pixel.x * float(i), uv.y);
            vec4 sample = texture(iChannel0, uvmod);
            fragColor += sample / (0.125 * float(i + 1));
        }
        
        fragColor.x = clamp(fragColor.x, 0.0, 1.0);
        fragColor.yz = clamp(fragColor.yz, -1.0, 1.0);
    }
]], { iChannel0 = vhs_dropout })

local vhs_dropout_combined = riteg.create_shader(base_wide, base_tall, common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord.xy / iResolution.xy;
        vec4 color = texture(iChannel0, uv);
        vec4 dropout = texture(iChannel1, uv);
        
        fragColor.x = color.x + dropout.x;
        fragColor.y = mix(color.y, 0.125 * dropout.y * dropout.x, dropout.x);
        fragColor.z = mix(color.z, 0.125 * dropout.z * dropout.x, dropout.x);
        fragColor.w = 1.0;
    }
]], { iChannel0 = vhs_sharpen, iChannel1 = vhs_dropout_blur })

local vhs_headswitch = riteg.create_shader(base_wide, base_tall, common_glsl .. [[
    #define P0 0.003125
    #define P1 30.00000
    #define P2 3.000000
    #define P3 0.062500
    #define P4 0.500000

    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        uint rngStateRow = Common_newRngState(vec2(0.0, fragCoord.y));
        vec2 uv = fragCoord.xy / iResolution.xy;

        if(uv.y <= P0) {
            float amount = 1.0 / exp((P0 - uv.y) * 10.0 * P1 + P2);
            float kjig = 1.0 / exp((P0 - uv.y) * P1 + P2 - 0.5);

            float jiggle = P3 * kjig * Common_randomFloat(rngStateRow);
            vec2 uvmod = vec2(uv.x - amount + jiggle, uv.y);

            if((uvmod.x < 0.0) || (uvmod.x > 1.0))
                fragColor = vec4(0.0, 0.0, 0.0, 1.0);
            else fragColor = texture(iChannel0, uvmod);

            fragColor.yz *= clamp(1.0 - 25.0 * kjig, 0.0, 1.0);
        }
        else {
            float jiggle = P4 * Common_randomFloat(rngStateRow) / iResolution.x;
            vec2 uvmod = vec2(uv.x + jiggle, uv.y);
            if((uvmod.x < 0.0) || (uvmod.x > 1.0))
                fragColor = vec4(0.0, 0.0, 0.0, 1.0);
            else fragColor = texture(iChannel0, uvmod);
        }
    }
]], { iChannel0 = vhs_dropout_combined })

local vhs_tracking = riteg.create_shader(base_wide, base_tall, common_glsl .. [[
    #define P0 50.0
    #define P1 1.0
    #define P2 0.5

    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        uint rngState = Common_newRngState(fragCoord);
        uint rngStateRow = Common_newRngState(vec2(0.0, fragCoord.y));

        vec2 uv = fragCoord / iResolution.xy;
        float pixel = 1.0 / iResolution.x;

        float factor = exp(-0.5 * P0 * uv.y);
        float wave = P1 * Common_randomFloat(rngStateRow);
        vec4 color = texture(iChannel0, uv + vec2(factor * wave * pixel, 0.0));

        float ymod = P2 * dot(color.yz, color.yz);
        float imod = ymod * (2.0 * Common_randomFloat(rngState) - 1.0);
        float qmod = ymod * (2.0 * Common_randomFloat(rngState) - 1.0);

        fragColor.x = color.x;
        fragColor.y = mix(color.y, imod, factor);
        fragColor.z = mix(color.z, qmod, factor);
        fragColor.w = color.w;
    }
]], { iChannel0 = vhs_headswitch })

local post_vcr_rgb = riteg.create_shader(base_wide, base_tall, yiq_to_rgb_src, { iChannel0 = vhs_tracking })
local ntsc_post_vcr_encode = riteg.create_shader(2.0 * base_wide, base_tall, ntsc_encode_src(1), { iChannel0 = post_vcr_rgb })
local ntsc_post_vcr_decode_y = riteg.create_shader(2.0 * base_wide, base_tall, ntsc_decode_y_src, { iChannel0 = ntsc_post_vcr_encode })
local ntsc_post_vcr_decode_iq = riteg.create_shader(2.0 * base_wide, base_tall, ntsc_decode_iq_src, { iChannel0 = ntsc_post_vcr_decode_y })

local final_rgb = riteg.create_shader(out_wide, out_tall, common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord / iResolution.xy;
        vec4 color = texture(iChannel0, uv);
        fragColor.rgb = color.xyz * YIQ_TO_RGB;
        fragColor.a = 1.0;
    }
]], { iChannel0 = ntsc_post_vcr_decode_iq })

riteg.set_output_source(final_rgb)
