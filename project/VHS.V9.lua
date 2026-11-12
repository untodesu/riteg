-- With a new iteration of RITEG's shader API, a new iteration
-- of the VHS filter comes. This time RITEG.V2b features a complete
-- rewrite of the pipeline with less shader passes and more flexible
-- configuration thanks to the RITEG.V2b running as a Lua runtime

-- This time the pipeline relies more on existing shaders
-- that are just ported to better suit the purpose of a multi-pass VHS filter

-- V9 is V8 plus the medium. V8 only had a deck: noise was a fresh white
-- hash every frame, so nothing tied it to a tape. V9 adds one physical
-- premise and derives everything from it: a frame fits into one helical
-- track, so along the track runs the raster of the frame and across the
-- tracks runs time, i.e. the frame number. A worn or dirty patch of tape is
-- therefore a horizontal band a few rows tall, and it recurs on the frames
-- whose tracks it covers. The field lives on the tape and never morphs;
-- only the noise read out of it is new every frame. A bad area raises the
-- threshold of that noise instead of painting a blob, which is what makes
-- the damage look stationary while the static still dances.

-- ATTRIBUTION ATTRIBUTION ATTRIBUTION ATTRIBUTION
-- ATTRIBUTION ATTRIBUTION ATTRIBUTION ATTRIBUTION
-- ATTRIBUTION ATTRIBUTION ATTRIBUTION ATTRIBUTION
-- ATTRIBUTION ATTRIBUTION ATTRIBUTION ATTRIBUTION
--  VHS Compression: https://www.shadertoy.com/view/tsfXWj
--  NTSC Filter: https://www.shadertoy.com/view/wlScWG

riteg.log_warning("VHS.V9 filter by untodesu")
riteg.log_warning("Based on mostly own work from 2022 with the following bits:")
riteg.log_warning("- VHS compression: https://www.shadertoy.com/view/tsfXWj by mpalko")
riteg.log_warning("- NTSC filter: https://www.shadertoy.com/view/wlScWG by Hatchling")

-- Specify base resolution at which the filter
-- actually operates. This has nothing to do with
-- the output resolution, although output defaults to base
local base_wide <const> = 1280
local base_tall <const> = 480

-- Specify the output resolution of the filter
local out_wide = math.min(math.max(riteg.get_option_number("output-width", base_wide / 2), 320), 2560)
local out_tall = math.min(math.max(riteg.get_option_number("output-height", base_tall), 240), 1920)

-- The downsample pass only keeps 333 columns of luminance and 40 columns
-- of chroma. Three extra columns keep the black margin that bicubic
-- sampling used to see at the right edge of the old full-width target.
local downsample_wide <const> = 336

-- Create a shader node from a source. `channels` maps channel indices to sources.
local function make_pass(source, width, height, channels, options)
    local node = riteg.node(riteg.program(source), width, height, options)

    if channels then
        for channel, input in pairs(channels) do
            node:input(channel, input)
        end
    end

    return node
end

-- Ensure we set up the random generator with a unique
-- seed every time so the generational loss does not just
-- overlay; I had an idea to use the seed in shaders but didn't
-- read into lua documentation enough to find out one has to initialize
-- the RNG before calling math.random [untodesu, 2025-09-05]
math.randomseed(os.time())

local function option(name, fallback, lo, hi)
    return math.min(math.max(riteg.get_option_number(name, fallback), lo), hi)
end

-- One run is one tape played on one deck. -seed makes the pair reproducible,
-- -tape and -deck override a single side of it. Wear is derived from the tape
-- id by default, so two different tapes are worn differently without any
-- extra arguments.
local base_seed = math.floor(option("seed", math.random(0, 65535), 0, 65535))
local tape_seed = math.floor(option("tape", base_seed, 0, 65535))
local deck_seed = math.floor(option("deck", (base_seed * 1664525 + 1013904223) % 65536, 0, 65535))

-- Default is a new tape: enough wear for the deck to show its character, but
-- no visible bands or dropouts. Use -wear 1.0 and up for a used one.
local tape_wear = option("wear", 0.22 + 0.10 * (tape_seed % 11) / 10.0, 0.0, 2.0)
local tape_dropout = option("dropout", 0.02, 0.0, 0.2)
local tape_grain = option("grain", 0.04, 0.0, 0.3)
local tape_tracking = option("tracking", 0.02, 0.0, 0.2)
local tape_wobble = option("wobble", 0.02, 0.0, 0.1)
local tape_breath = option("breath", 0.06, 0.0, 0.1)
local tape_headswitch = option("headswitch", 0.02, 0.0, 0.1)
local tape_chroma_phase = option("chroma-phase", 0.0625, 0.0, 1.0)
local tape_frame_bounce = option("frame-bounce", 0.05, 0.0, 0.2)
local tape_frame_step = option("frame-step", 2.0, 0.5, 12.0)
local tape_doc = option("doc", 1.0, 0.0, 1.0)
local tape_sparkle = option("sparkle", 0.8, 0.0, 1.0)
local tape_scratch = option("scratch", 0.0, 0.0, 1.0)

-- A defect that is h rows tall covers about 4*h tracks at 480 lines, so it
-- recurs for about 4*h frames. This is the one number that ties the vertical
-- size of a band to its lifetime; lower it for scratches that run along the
-- track, raise it for blobs that bleed across it.
local tape_track_correlation <const> = 4.0

-- Along-track slope of a band. Physics gives one row across the frame width
-- (1/480); a little more reads better without turning bands into diagonals.
local tape_slant <const> = 2.0 / base_tall

riteg.log_info(string.format(
    "VHS hop: tape=%d deck=%d wear=%.3f dropout=%.4f grain=%.4f tracking=%.4f slant=%.5f wobble=%.5f breath=%.4f headswitch=%.4f chroma=%.4f bounce=%.4f step=%.1f doc=%.3f sparkle=%.3f scratch=%.3f",
    tape_seed, deck_seed, tape_wear, tape_dropout, tape_grain, tape_tracking, tape_slant, tape_wobble, tape_breath, tape_headswitch, tape_chroma_phase, tape_frame_bounce, tape_frame_step, tape_doc, tape_sparkle, tape_scratch))

-- A bunch of GLSL code that is shared between all the
-- passes in the filter pipeline; this contains bits from
-- both VHS compression, NTSC filter and my own code
local common_glsl <const> = string.format([[
    const float PI = 3.14159265359;

    const uint TAPE_SEED = %dU;
    const uint DECK_SEED = %dU;

    // Physical condition of the tape, from the tape id or -wear.
    const float TAPE_WEAR = %.4f;
    const float TAPE_DROPOUT = %.5f;
    const float TAPE_GRAIN = %.5f;
    const float TAPE_DRIFT = %.5f;
    const float TAPE_SLANT = %.5f;

    // Rows in one frame. The wear octaves and the scratch line use it to
    // stay in tape coordinates at any output resolution.
    const float TAPE_ROWS = 480.0;

    // Time-base error: how far a bad patch can displace a line start,
    // as a fraction of the frame width.
    const float TAPE_WOBBLE = %.5f;

    // Slow head-to-tape contact breathing: global luma/grain modulation.
    const float TAPE_BREATH = %.4f;

    // How far wear extends the head-switch zone at the frame edge.
    const float TAPE_HEADSWITCH = %.4f;

    // How much wear adds chroma phase/frequency error, in radians.
    const float TAPE_CHROMA_PHASE = %.4f;

    // Whole-frame sync drift: a tape/deck that loses frame lock lets the
    // picture float vertically. Fraction of frame height.
    const float TAPE_FRAME_BOUNCE = %.4f;

    // Frame lock slips in whole lines, so the offset snaps in steps of this
    // many lines instead of gliding.
    const float TAPE_FRAME_STEP = %.1f;

    // How much of a dropout the compensator replaces with the previous line
    // before the independent white sparkle is added on top.
    const float TAPE_DOC = %.4f;
    const float TAPE_DOC_NOISE = 0.15;

    // A tape is never perfectly clean: TAPE_DROPOUT_BASE keeps a little
    // grain everywhere and lets rare dropouts appear on a fresh tape.
    // TAPE_DROPOUT_WEAR adds general dropouts as the tape wears, so a bad
    // tape snows all over and not only in its bands.
    const float TAPE_FLOOR = 0.02;
    const float TAPE_DROPOUT_BASE = 0.0002;
    const float TAPE_DROPOUT_WEAR = 0.02;

    // How much of a dropout is replaced by noise instead of the held line.
    // At 1 the signal is gone; below 1 some of the compensated line shows.
    const float TAPE_SPARKLE = %.4f;

    // A scratch is a dead line: the head reads noise instead of signal, so
    // it goes through the same dropout path. -scratch scales how many rows
    // of tape can be scratched; wear scales it too, and 0 turns it off.
    const float TAPE_SCRATCH = %.4f;

    // Tracks per row: a defect h rows tall covers about 4*h tracks, so it
    // recurs on 4*h consecutive frames. One knob ties height to lifetime.
    const float TAPE_TRACK_CORRELATION = %.1f;

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
    const float SATURATION = 4.0; // Saturation control
    const float WINDOW_BIAS = 0.0; // Offsets shape of window. This can make artifacts smear to one side or the other.

    const float VHS_CUTOFF = 0.01;
    const int VHS_DROPOUT_BLUR_STEPS = 64;
    // A dropout is a loss over a run of pixels, never a single pixel.
    const int VHS_DROPOUT_MIN_STEPS = 32;

    const vec2 VHS_MAXRES_Y = vec2(333.0, 480.0);
    const vec2 VHS_MAXRES_IQ = vec2(40.0, 480.0);
    const vec2 VHS_BLUR_AMT = vec2(0.2, 0.2);

    const float GAMMA_CORRECTION = 0.707;

    // VCR output is a little darker than the source, and every copy loses
    // a bit more level. 0.98 per generation is about 6 percent down by the fourth pass
    // relative to the first: noticeable, but not a fade to black.
    const float OUTPUT_LEVEL_LOSS = 0.98;

    const mat3 RGB_TO_YIQ = mat3(0.299, 0.587, 0.114, 0.595, -0.274, -0.3213, 0.2115,-0.5227, 0.3112);
    const mat3 YIQ_TO_RGB = mat3(1.0, 0.956, 0.619, 1.0, -0.272, -0.647, 1.0, -1.106, 1.703);

    // Empirical inverse of the flat-color transfer of one full pass
    // (two NTSC encode/decode pairs plus the saturation stage) at
    // SATURATION = 4. Without it the pass has a net chroma gain > 1 and
    // feedback pumps colors until they clip. With it a flat patch comes
    // back at almost the same color, so generations only add the intended
    // spatial/analog artifacts instead of a hue shift.
    // Recompute this if SATURATION or the NTSC constants change.
    const mat3 COLOR_CORRECTION = mat3(
         0.912426188, -0.010643677,  0.239216447,
        -0.052850288,  1.101521626, -0.371550428,
         0.138775636, -0.090454783,  1.119551022);

    uint Common_hashState(vec2 uv, uint entropy, uint seed)
    {
        return uint(uint(uv.x) * 1973U + uint(uv.y) * 9277U + entropy * 34961U + seed * 41077U) | 1U;
    }

    // Tape defects are the same on every frame; only the track index picks
    // which of them is under the head, and that is the frame number.
    uint Common_tapeRng(vec2 uv, uint entropy)
    {
        return Common_hashState(uv, entropy, TAPE_SEED);
    }

    uint Common_newRngStateEx(vec2 uv, uint entropy)
    {
        return uint(uint(uv.x) * 1973U + uint(uv.y) * 9277U + uint(iFrame) * 26699U + entropy * 34961U + DECK_SEED * 41077U) | 1U;
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

    float Tape_corner(vec2 cell, uint salt)
    {
        uint state = Common_tapeRng(cell, salt);
        return Common_randomFloat(state);
    }

    // 2D value noise in (along the track, track index). The two axes have
    // very different scales on purpose: that is what makes a defect a band.
    float Tape_noise(vec2 p, uint salt)
    {
        vec2 i = floor(p);
        vec2 f = fract(p);
        f = f * f * (3.0 - 2.0 * f);

        float c00 = Tape_corner(i + vec2(0.0, 0.0), salt);
        float c10 = Tape_corner(i + vec2(1.0, 0.0), salt);
        float c01 = Tape_corner(i + vec2(0.0, 1.0), salt);
        float c11 = Tape_corner(i + vec2(1.0, 1.0), salt);

        return mix(mix(c00, c10, f.x), mix(c01, c11, f.x), f.y);
    }

    // Mistracking moves the whole pattern up and down the picture over a few
    // seconds. The tape does not change; the head just reads it offset.
    float Tape_drift()
    {
        float n = Tape_noise(vec2(float(iFrame) / (TAPE_TRACK_CORRELATION * 32.0), 0.5), 40U);
        return 2.0 * n - 1.0;
    }

    // Head-to-tape contact breathes over seconds: the whole frame slowly
    // gains and loses level. One slow noise, so it is global and smooth.
    float Tape_breath()
    {
        float n = Tape_noise(vec2(float(iFrame) / 72.0, 0.5), 60U);
        return 2.0 * n - 1.0;
    }

    // Whole-frame vertical sync offset, in frame fractions. Constant per
    // frame; frame lock slips in whole lines, so it snaps in steps.
    float Tape_frameOffset()
    {
        float amount = smoothstep(0.40, 1.0, TAPE_WEAR);
        float slow = 2.0 * Tape_noise(vec2(float(iFrame) / 72.0, 0.5), 70U) - 1.0;
        float fast = 2.0 * Tape_noise(vec2(float(iFrame) / 12.0, 0.5), 71U) - 1.0;
        float bounce = (0.6 * slow + 0.4 * fast) * TAPE_FRAME_BOUNCE * amount;
        return floor(bounce * TAPE_ROWS / TAPE_FRAME_STEP + 0.5) * TAPE_FRAME_STEP / TAPE_ROWS;
    }

    // Wear as a function of the frame. The octaves are 64, 16 and 4 rows
    // tall, so they are 480/h cycles per frame along the track, and each
    // lives TAPE_TRACK_CORRELATION*h frames across the tracks. The 0.5
    // bias keeps the noise coordinate positive so uint() is well defined.
    float Tape_wear(vec2 uv)
    {
        float s = 0.5 + uv.y + TAPE_SLANT * uv.x + TAPE_DRIFT * Tape_drift();
        float k = float(iFrame);

        float n = 0.55 * Tape_noise(vec2(s *   7.5, k / (TAPE_TRACK_CORRELATION * 64.0)), 20U)
                + 0.30 * Tape_noise(vec2(s *  30.0, k / (TAPE_TRACK_CORRELATION * 16.0)), 21U)
                + 0.15 * Tape_noise(vec2(s * 120.0, k / (TAPE_TRACK_CORRELATION *  4.0)), 22U);

        float floorWear = TAPE_FLOOR * step(0.005, TAPE_WEAR);
        return clamp(TAPE_WEAR * smoothstep(0.52, 0.76, n) + floorWear, 0.0, 1.0);
    }

    // Time-base error: a bad patch does not only add noise, it makes the
    // line start arrive early or late. Neighbouring lines move together, and
    // the pattern is stable for a few frames, so a band tears and wobbles
    // instead of flickering. Applied to the input, not overlaid on the
    // result: the picture itself is displaced.
    float Tape_shift(vec2 uv)
    {
        float s = 0.5 + uv.y;
        float k = float(iFrame);

        float fast = Tape_noise(vec2(s * 240.0, k / (TAPE_TRACK_CORRELATION * 2.0)), 30U);
        float slow = Tape_noise(vec2(s *  48.0, k / (TAPE_TRACK_CORRELATION * 8.0)), 31U);

        return (0.65 * (2.0 * fast - 1.0) + 0.35 * (2.0 * slow - 1.0)) * TAPE_WOBBLE;
    }

    // A scratch is a dead line: no signal, so the dropout path fills it with
    // noise. Its row is fixed on the tape, but the head only loses the track
    // there for a while, so each one fades in and out over a few seconds.
    // Wear decides how many rows can be scratched, -scratch scales that.
    float Tape_scratch(vec2 uv)
    {
        float row = floor(uv.y * TAPE_ROWS * 0.5);

        uint posState = Common_tapeRng(vec2(0.0, row), 50U);
        float pos = Common_randomFloat(posState);
        float rate = 0.02 * TAPE_SCRATCH * clamp(TAPE_WEAR, 0.0, 1.0);
        float canScratch = step(1.0 - rate, pos);

        float life = Tape_noise(vec2(row * 0.7, float(iFrame) / 240.0), 51U);
        float active = smoothstep(0.35, 0.65, life);

        return canScratch * active;
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
            // GLSL mat3 is column-major, so the row-major constants above
            // have to be applied as row vectors: v * M == M^T * v.
            vec3 yiqSample = clamp(texture(sampler, uvWithOffset).rgb, vec3(0.0), vec3(1.0)) * RGB_TO_YIQ;

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
        float decoded = 0.0;

        for(int i = -DECODE_WINDOW_WIDTH; i <= DECODE_WINDOW_WIDTH; ++i) {
            float window = NTSC_cosineWindow(float(i) / float(DECODE_WINDOW_WIDTH+1));

            vec2 uvWithOffset = vec2(uv.x + float(i) * pixelWidth, uv.y);

            float sinc = NTSC_sinc(float(i) / AM_DECODE_HIGHPASS_WAVELENGTH) / AM_DECODE_HIGHPASS_WAVELENGTH;
            float encodedSample = texture(sampler, uvWithOffset).x;
            
            decoded += encodedSample * sinc * window;
        }
        
        return decoded * sin(phaseAM) * 4.0;
    }

    vec3 NTSC_fullDecodeYIQ(sampler2D sampler, in vec2 uv, in float pixelWidth, uint rngRow, bool alternatePhase, float tapeWear)
    {
        uint seed = rngRow;
        vec2 originalUV = uv;
        
        vec3 rowNoiseIntensity = vec3(Common_randomFloat(seed), Common_randomFloat(seed), Common_randomFloat(seed));
        rowNoiseIntensity = pow(rowNoiseIntensity, vec3(500.0)) * 0.125;
        
        float phaseNoise = Common_randomFloat(seed) * 2.0 - 1.0;
        phaseNoise *= rowNoiseIntensity.y * 0.5 * 3.1415927 * NOISE_STRENGTH;

        float frequencyNoise = Common_randomFloat(seed) * 2.0 - 1.0;
        frequencyNoise *= rowNoiseIntensity.z * 0.5 * 3.1415927 * NOISE_STRENGTH;

        // Worn tape loses chroma phase lock: each line gets a hue offset and
        // a small frequency error, so the error drifts along the line too.
        float wearPhase = (Common_randomFloat(seed) * 2.0 - 1.0) * TAPE_CHROMA_PHASE * tapeWear;
        float wearFrequency = (Common_randomFloat(seed) * 2.0 - 1.0) * TAPE_CHROMA_PHASE * 0.01 * tapeWear;
        phaseNoise += wearPhase;
        frequencyNoise += wearFrequency;
        
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
]], tape_seed, deck_seed, tape_wear, tape_dropout, tape_grain, tape_tracking, tape_slant, tape_wobble, tape_breath, tape_headswitch, tape_chroma_phase, tape_frame_bounce, tape_frame_step, tape_doc, tape_sparkle, tape_scratch, tape_track_correlation)

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
        fragColor.rgb = yiq * YIQ_TO_RGB;
        fragColor.w = 1.0;
    }
]]

-- Tape noise is the deck reading the RF off the tape. `tape_gain` is the
-- only place the wear mask enters the signal chain: the recording is made
-- once, so the first encode carries the tape's noise and the later one does
-- not. The mask stays a threshold on the same noise events instead of a
-- separate overlay, so a bad band gets denser specks, not a colored patch.
local function ntsc_encode_src(step, tape_gain)
    local wear_decl = "float wear = 0.0;"
    if tape_gain > 0.0 then
        wear_decl = "float wear = texture(iChannel1, uv).x;"
    end

    return common_glsl .. string.format([[
        void mainImage(out vec4 fragColor, in vec2 fragCoord)
        {
            uint rngState = Common_newRngStateEx(fragCoord, %dU);
            uint rngStateRow = Common_newRngStateEx(vec2(0.0, fragCoord.y), %dU);
            
            vec2 uv = fragCoord / iResolution.xy;
            vec2 pixelSize = 1.0 / iResolution.xy;
            %s
            
            float encoded = NTSC_fullEncode(iChannel0, uv, pixelSize.x * NTSC_SCALE, int(fragCoord.y) %% 2 == 0);
            
            float snowNoise = Common_randomFloat(rngState) - 0.5;
            float sineNoise = sin(uv.x * 200.0 + uv.y * -50.0 + fract(iTime * iTime) * 3.1415927 * 2.0) * 0.065;
            float saltPepperNoise = Common_randomFloat(rngState) * 2.0 - 1.0;
    
            saltPepperNoise = sign(saltPepperNoise) * pow(abs(saltPepperNoise), 200.0) * 10.0;
        
            float rowNoise = Common_randomFloat(rngStateRow) * 2.0 - 1.0;
            rowNoise *= 0.1;   
        
            float rowSaltPepper = Common_randomFloat(rngStateRow) * 2.0 - 1.0;
            rowSaltPepper = sign(rowSaltPepper) * pow(abs(rowSaltPepper), 200.0) * 1.0;
            
            float noise = snowNoise + saltPepperNoise + sineNoise + rowNoise + rowSaltPepper;
            encoded += noise * NOISE_STRENGTH * (1.0 + %f * wear); 
            
            fragColor.xyz = vec3(encoded);
            fragColor.w = 1.0;
        }
    ]], step, step, wear_decl, tape_gain)
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
        float wear = texture(iChannel1, uv).x;
        
        uint rngStateRow = Common_newRngState(vec2(0.0, fragCoord.y));

        fragColor.xyz = NTSC_fullDecodeYIQ(iChannel0, uv, pixelSize.x * NTSC_SCALE, rngStateRow, int(fragCoord.y) % 2 == 0, wear);
        fragColor.w = 1.0;
    }
]]

-- The only expensive part of the tape model, and it runs on 32 columns: the
-- field varies along the track, and the frame is only wide because the
-- raster is. Consumers sample this mask at their own uv. R is wear, G is
-- the horizontal time-base error of that line.
local tape_mask = make_pass(common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord / iResolution.xy;
        float wear = Tape_wear(uv);
        fragColor = vec4(wear, Tape_shift(uv) * wear, 0.0, 1.0);
    }
]], 32, base_tall / 2, nil, { format = "rg16f", filter = "linear" })

local gamma_correct = make_pass(common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord / iResolution.xy;
        vec2 tape = texture(iChannel1, uv).xy;
        // Whole-frame sync drift: the picture slides and wraps. The wrapped
        // rows are marked as dead reads in the dropout pass, so they do not
        // need their own noise here.
        float bounce = Tape_frameOffset();
        vec4 color = texture(iChannel0, vec2(uv.x + tape.y, fract(uv.y + bounce)));
        fragColor.rgb = pow(color.rgb, vec3(1.0 / GAMMA_CORRECTION));
        fragColor.a = color.a;
    }
]], base_wide, base_tall, { [0] = riteg.get_image_source(), [1] = tape_mask })

local ntsc_pre_vcr_encode = make_pass(ntsc_encode_src(0, 8.0), 2.0 * base_wide, base_tall, { [0] = gamma_correct, [1] = tape_mask }, { format = "r16f" })
local ntsc_pre_vcr_decode_y = make_pass(ntsc_decode_y_src, 2.0 * base_wide, base_tall, { [0] = ntsc_pre_vcr_encode }, { format = "r16f" })
local ntsc_pre_vcr_decode_iq = make_pass(ntsc_decode_iq_src, 2.0 * base_wide, base_tall, { [0] = ntsc_pre_vcr_decode_y, [1] = tape_mask })

local vhs_cutoff = make_pass(common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord.xy / iResolution.xy;
        vec4 color = texture(iChannel0, uv);
        fragColor = color * step(VHS_CUTOFF, 1.0 - uv.x);
    }
]], base_wide, base_tall, { [0] = ntsc_pre_vcr_decode_iq })

local vhs_downsample = make_pass(common_glsl .. [[
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
]], downsample_wide, base_tall, { [0] = vhs_cutoff })

local vhs_upsample = make_pass(common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord / iResolution.xy;
        
        // The downsample target packs luminance and chroma at different
        // effective widths, so the data scale comes from the channel size.
        vec2 channelSize = vec2(textureSize(iChannel0, 0));

        vec2 resLuminance = min(VHS_MAXRES_Y, channelSize);
        vec2 resChroma = min(VHS_MAXRES_IQ, channelSize);
        
        vec2 uvLuminance = uv * (resLuminance / channelSize);
        vec2 uvChroma = uv * (resChroma / channelSize);
        
        float luminance = VHS_textureBicubic(iChannel0, uvLuminance).x;
        vec2 chroma = VHS_textureBicubic(iChannel0, uvChroma).yz;
        
        fragColor = vec4(luminance, chroma, 1.0);
    }
]], base_wide, base_tall, { [0] = vhs_downsample })

local vhs_lowerpass = make_pass(lowerpass_shader(1.0, 0.0), base_wide, base_tall, { [0] = vhs_upsample })

local vhs_upsample_combined = make_pass(common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord / iResolution.xy;
        vec4 vhs_upsample = texture(iChannel0, uv);
        vec4 vhs_lowerpass = texture(iChannel1, uv);
        
        fragColor.xyz = 3.0 * vhs_upsample.xyz - 2.0 * vhs_lowerpass.xyz;
        fragColor.w = 1.0;
    }
]], base_wide, base_tall, { [0] = vhs_upsample, [1] = vhs_lowerpass })

local vhs_sharpen = make_pass(common_glsl .. [[
    #define SHARPEN_AMOUNT 1.0

    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord.xy / iResolution.xy;
        vec4 color = texture(iChannel0, uv);
        color += SHARPEN_AMOUNT * dFdx(color);
        color += SHARPEN_AMOUNT * dFdy(color);
        fragColor = color;
    }
]], base_wide, base_tall, { [0] = vhs_upsample_combined })

-- Dropouts are a loss of signal over a finite time, i.e. over a run of
-- pixels along the raster, not over one pixel. Hashing a 5 px cell keeps a
-- run contiguous; the next pass smears it into a streak. They need a worse
-- patch than the grain does, so they threshold the same wear mask higher
-- and sit inside the noisy band instead of covering all of it. A scratch is
-- the same thing at full density over one row, so it just raises the
-- threshold there instead of growing a second noise generator.
local vhs_dropout = make_pass(common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord.xy / iResolution.xy;
        float wear = texture(iChannel0, uv).x;

        vec2 cell = floor(vec2(uv.x * 256.0, fragCoord.y));
        uint rngState = Common_newRngState(cell);

        float density = TAPE_DROPOUT * smoothstep(0.25, 0.80, wear) * (0.4 + 0.6 * (1.0 - uv.y));
        float baseDropout = TAPE_DROPOUT * (TAPE_DROPOUT_BASE
            + TAPE_DROPOUT_WEAR * smoothstep(0.30, 1.0, TAPE_WEAR))
            * step(0.005, TAPE_WEAR);
        density = max(density, baseDropout);
        density = max(density, Tape_scratch(uv));

        // Rows where the frame offset wrapped show the next/previous track's
        // stub, read from the side. There is mostly gap between tracks there,
        // so treat the whole band as one dead read and let the dropout path
        // supply its snow, DOC and chroma.
        float frameY = uv.y + Tape_frameOffset();
        density = max(density, step(1.0, frameY) + step(frameY, 0.0));

        fragColor.x = step(1.0 - clamp(density, 0.0, 1.0), Common_randomFloat(rngState));

        uint rngI = Common_newRngState(cell + vec2(17.0, 0.0));
        uint rngQ = Common_newRngState(cell + vec2(43.0, 0.0));
        fragColor.y = 2.0 * Common_randomFloat(rngI) - 1.0;
        fragColor.z = 2.0 * Common_randomFloat(rngQ) - 1.0;
        fragColor.w = 1.0;
    }
]], base_wide, base_tall, { [0] = tape_mask })

-- The length is per pixel, as in V8: neighbouring pixels disagree, so a run
-- breaks into white and coloured fragments instead of one solid bar. The
-- floor keeps any fragment at least VHS_DROPOUT_MIN_STEPS long.
local vhs_dropout_blur = make_pass(common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        uint rngState = Common_newRngState(fragCoord);

        vec2 uv = fragCoord.xy / iResolution.xy;
        vec2 pixel = 1.0 / iResolution.xy;
        float wear = texture(iChannel1, uv).x;

        fragColor = vec4(0.0, 0.0, 0.0, 1.0);

        int steps = int(max(float(VHS_DROPOUT_MIN_STEPS),
            VHS_DROPOUT_BLUR_STEPS * (0.2 + 0.8 * wear) * Common_randomFloat(rngState)));

        for(int i = 0; i < steps; ++i) {
            vec2 uvmod = vec2(uv.x - pixel.x * float(i), uv.y);
            vec4 sample = texture(iChannel0, uvmod);
            fragColor += sample / (0.125 * float(i + 1));
        }
        
        fragColor.x = clamp(fragColor.x, 0.0, 1.0);
        fragColor.yz = clamp(fragColor.yz, -1.0, 1.0);
    }
]], base_wide, base_tall, { [0] = vhs_dropout, [1] = tape_mask })

-- The baseband side of wear. Oxide loss first: the band loses fine detail,
-- level and chroma, and gets its own chroma noise (VHS colour dies first).
-- Then the dropout layer: the compensator holds the previous line, but a
-- dropout is lost signal, so luma and chroma become coloured snow. A scratch
-- is the same loss for a whole row, at full strength.
local vhs_dropout_combined = make_pass(common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord.xy / iResolution.xy;
        vec2 pixel = 1.0 / iResolution.xy;

        vec4 color = texture(iChannel0, uv);
        vec4 dropout = texture(iChannel1, uv);
        float wear = texture(iChannel2, uv).x;

        uint rngState = Common_newRngState(fragCoord);
        float n1 = Common_randomFloat(rngState) - 0.5;
        float n2 = Common_randomFloat(rngState) - 0.5;
        float n3 = Common_randomFloat(rngState) - 0.5;
        float grain = (Common_randomFloat(rngState) - 0.5) * TAPE_GRAIN * wear;

        // Snow lives on an 8 px cell so it survives the decode lowpass and
        // the output downsample; finer noise just averages into a grey line.
        uint snowState = Common_newRngState(vec2(floor(uv.x * iResolution.x * 0.125), fragCoord.y));
        float snow = Common_randomFloat(snowState);

        // Oxide loss: worn oxide cannot carry the high frequencies and the
        // AGC pulls the level down instead of recovering it.
        vec3 left = texture(iChannel0, uv - vec2(pixel.x, 0.0)).xyz;
        vec3 right = texture(iChannel0, uv + vec2(pixel.x, 0.0)).xyz;
        vec3 soft = 0.25 * left + 0.5 * color.xyz + 0.25 * right;
        float oxide = smoothstep(0.25, 0.90, wear);

        vec3 base = mix(color.xyz, soft, 0.7 * oxide);
        base.x *= 1.0 - 0.12 * oxide;
        base.yz *= 1.0 - 0.50 * oxide;
        base.yz += vec2(n2, n3) * 0.15 * oxide;

        // What the one-line delay holds, at the same horizontal position.
        vec4 prev = texture(iChannel0, uv - vec2(0.0, pixel.y));

        // The substituted line is noisy too, otherwise the hold looks clean.
        vec3 held = prev.xyz + vec3(n1, n2, n3) * TAPE_DOC_NOISE;

        float mask = clamp(dropout.x, 0.0, 1.0);
        vec3 result = mix(base, held, mask * TAPE_DOC);

        // A dropout is a loss of signal, so it reads as coloured snow, not
        // as one flat white bar: luma and chroma are both noise here.
        float scratch = Tape_scratch(uv);
        float streak = mask * (1.0 - scratch);

        // The wrapped band is a dead read, but its noise should follow the
        // signal level: on black it stays dark instead of flashing white,
        // on real content it still reads as snow.
        float frameY = uv.y + Tape_frameOffset();
        float band = step(1.0, frameY) + step(frameY, 0.0);
        float level = clamp(result.x, 0.0, 1.0);
        float bandDead = mix(1.0, 0.10 + 0.90 * level, band);

        float dead = clamp(streak * TAPE_SPARKLE * 10.0, 0.0, 1.0) * bandDead;
        result.x = mix(result.x, snow, dead);
        result.yz = mix(result.yz, vec2(n2, n3) * 1.2, streak * (1.0 - 0.90 * band));

        // A scratch takes the whole row down, so it is snow at full strength.
        result.x = mix(result.x, snow, scratch);
        result.yz = mix(result.yz, vec2(n2, n3) * 0.12, scratch);

        // Contact breathing: a slow global level wobble that also lifts the
        // grain while the head is pressed harder against the tape.
        float breath = Tape_breath() * TAPE_BREATH;
        result.x = result.x * (1.0 + breath) + grain * (1.0 + 2.0 * breath);

        fragColor = vec4(result, 1.0);
    }
]], base_wide, base_tall, { [0] = vhs_sharpen, [1] = vhs_dropout_blur, [2] = tape_mask })

local vhs_headswitch = make_pass(common_glsl .. [[
    #define P0 0.003125
    #define P1 30.00000
    #define P2 3.000000
    #define P3 0.062500
    #define P4 0.500000

    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        uint rngStateRow = Common_newRngState(vec2(0.0, fragCoord.y));
        vec2 uv = fragCoord.xy / iResolution.xy;

        // A worn tape loses head-switch lock more easily, so the band of
        // shifted, desaturated lines at the frame edge grows with wear.
        float zone = P0 + TAPE_HEADSWITCH * clamp(TAPE_WEAR, 0.0, 1.0);

        if(uv.y <= zone) {
            float amount = 1.0 / exp((zone - uv.y) * 10.0 * P1 + P2);
            float kjig = 1.0 / exp((zone - uv.y) * P1 + P2 - 0.5);

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
]], base_wide, base_tall, { [0] = vhs_dropout_combined })

local vhs_tracking = make_pass(common_glsl .. [[
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
]], base_wide, base_tall, { [0] = vhs_headswitch })

local post_vcr_rgb = make_pass(yiq_to_rgb_src, base_wide, base_tall, { [0] = vhs_tracking })
local ntsc_post_vcr_encode = make_pass(ntsc_encode_src(1, 0.0), 2.0 * base_wide, base_tall, { [0] = post_vcr_rgb }, { format = "r16f" })
local ntsc_post_vcr_decode_y = make_pass(ntsc_decode_y_src, 2.0 * base_wide, base_tall, { [0] = ntsc_post_vcr_encode }, { format = "r16f" })
local ntsc_post_vcr_decode_iq = make_pass(ntsc_decode_iq_src, 2.0 * base_wide, base_tall, { [0] = ntsc_post_vcr_decode_y, [1] = tape_mask })

local final_rgb = make_pass(common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord / iResolution.xy;
        vec4 color = texture(iChannel0, uv);
        vec3 rgb = max(COLOR_CORRECTION * (color.xyz * YIQ_TO_RGB), vec3(0.0));
        fragColor.rgb = pow(rgb, vec3(GAMMA_CORRECTION)) * OUTPUT_LEVEL_LOSS;
        fragColor.a = 1.0;
    }
]], out_wide, out_tall, { [0] = ntsc_post_vcr_decode_iq })

riteg.output(final_rgb)
