-- With a new iteration of RITEG's shader API, a new iteration
-- of the VHS filter comes. This time RITEG.V2b features a complete
-- rewrite of the pipeline with less shader passes and more flexible
-- configuration thanks to the RITEG.V2b running as a Lua runtime

-- This time the pipeline relies more on existing shaders
-- that are just ported to better suit the purpose of a multi-pass VHS filter

-- V9 is V8 plus the medium. V8 only had a deck: noise was a fresh white
-- hash every frame, so nothing tied it to a tape. V9 adds a tape field in
-- tape coordinates: along the track (field line) and across the tracks
-- (track number). One helical track holds one field, so a frame is two
-- tracks; a worn patch has an along-track size (how many lines tall it is)
-- and an across-track size (how many tracks, i.e. fields, it lasts), and
-- those two sizes are independent. The field itself never changes; the head
-- just reads a new track each field. Dirt, dropout holes and scratches are
-- part of that field too, so they recur when their tracks come around
-- instead of being a fresh screen-space hash every frame.
--
-- The deck is a second, independent thing: head-switch width, wow/flutter,
-- tracking error, frame lock, TBC and the noise floor belong to it, not to
-- tape wear. -tape and -deck pick two sides of the pair, and their defaults
-- are derived from the same seed so a run is reproducible.
--
-- Presets set the whole parameter set at once; individual options still win:
--   -vhs-new      new tape, healthy deck (the default)
--   -vhs-used     light wear, still well tracked
--   -vhs-old      visible wear bands, occasional dropouts, older deck
--   -vhs-veryold  heavy wear, creases, scratches, weak servo, two dubs
--   -vhs-dub      clean-ish tape, three record/playback generations
--   -vhs-baddeck  new tape on a deck with bad tracking/TBC/frame lock
--   -vhs-damaged  physically damaged tape: crease, scratch, dropouts, mistracking
-- Example: -vhs-old -wear 0.4 -tracking 0.02

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

-- Named presets. A preset only supplies defaults: every individual option
-- still wins over it, so `-vhs-old -wear 0.4` is valid. Pass exactly one
-- preset; if several are present, the most severe one in `preset_order` wins.
-- The default (no preset) is the same idea as -vhs-new, with a little
-- seed-to-seed variation in the exact wear and deck numbers.
local presets = {
    ["vhs-new"] = {
        wear = 0.06, dropout = 0.004, grain = 0.020, scratch = 0.0, crease = 0.0,
        doc = 1.0, sparkle = 0.8, chroma_phase = 0.025, wobble = 0.006,
        band = 48.0, life = 96.0, position = 0.0,
        headswitch = 0.006, wow = 0.0002, flutter = 0.0001,
        framelock = 0.995, tbc = 0.96, noise = 0.65, tracking = 0.0,
        frame_bounce = 0.004, frame_step = 2.0, breath = 0.025,
        flag = 0.0, generations = 1,
    },
    ["vhs-used"] = {
        wear = 0.35, dropout = 0.02, grain = 0.035, scratch = 0.1, crease = 0.05,
        doc = 1.0, sparkle = 0.8, chroma_phase = 0.04, wobble = 0.010,
        band = 40.0, life = 80.0, position = 0.0,
        headswitch = 0.008, wow = 0.0004, flutter = 0.0002,
        framelock = 0.98, tbc = 0.90, noise = 0.8, tracking = 0.02,
        frame_bounce = 0.006, frame_step = 2.0, breath = 0.04,
        flag = 0.0, generations = 1,
    },
    ["vhs-old"] = {
        wear = 0.70, dropout = 0.05, grain = 0.050, scratch = 0.3, crease = 0.2,
        doc = 0.9, sparkle = 0.85, chroma_phase = 0.06, wobble = 0.015,
        band = 32.0, life = 64.0, position = 0.0,
        headswitch = 0.012, wow = 0.0008, flutter = 0.0004,
        framelock = 0.95, tbc = 0.80, noise = 1.0, tracking = 0.10,
        frame_bounce = 0.008, frame_step = 2.0, breath = 0.06,
        flag = 0.05, generations = 1,
    },
    ["vhs-veryold"] = {
        wear = 1.20, dropout = 0.10, grain = 0.070, scratch = 0.6, crease = 0.5,
        doc = 0.8, sparkle = 0.9, chroma_phase = 0.10, wobble = 0.025,
        band = 24.0, life = 48.0, position = 0.0,
        headswitch = 0.018, wow = 0.0015, flutter = 0.0008,
        framelock = 0.85, tbc = 0.65, noise = 1.3, tracking = 0.3,
        frame_bounce = 0.012, frame_step = 2.0, breath = 0.08,
        flag = 0.3, generations = 2,
    },
    ["vhs-dub"] = {
        wear = 0.15, dropout = 0.01, grain = 0.030, scratch = 0.05, crease = 0.05,
        doc = 1.0, sparkle = 0.8, chroma_phase = 0.04, wobble = 0.008,
        band = 40.0, life = 80.0, position = 0.0,
        headswitch = 0.008, wow = 0.0004, flutter = 0.0002,
        framelock = 0.97, tbc = 0.90, noise = 0.8, tracking = 0.0,
        frame_bounce = 0.006, frame_step = 2.0, breath = 0.04,
        flag = 0.0, generations = 3,
    },
    ["vhs-baddeck"] = {
        wear = 0.06, dropout = 0.004, grain = 0.020, scratch = 0.0, crease = 0.0,
        doc = 1.0, sparkle = 0.8, chroma_phase = 0.025, wobble = 0.006,
        band = 48.0, life = 96.0, position = 0.0,
        headswitch = 0.02, wow = 0.0015, flutter = 0.0008,
        framelock = 0.50, tbc = 0.20, noise = 1.0, tracking = 0.6,
        frame_bounce = 0.02, frame_step = 2.0, breath = 0.04,
        flag = 0.7, generations = 1,
    },
    ["vhs-damaged"] = {
        wear = 1.0, dropout = 0.12, grain = 0.060, scratch = 0.8, crease = 0.8,
        doc = 0.7, sparkle = 0.9, chroma_phase = 0.08, wobble = 0.02,
        band = 30.0, life = 70.0, position = 0.0,
        headswitch = 0.02, wow = 0.001, flutter = 0.0005,
        framelock = 0.70, tbc = 0.60, noise = 1.2, tracking = 0.5,
        frame_bounce = 0.015, frame_step = 2.0, breath = 0.06,
        flag = 0.5, generations = 1,
    },
}

local preset_order = { "vhs-new", "vhs-used", "vhs-old", "vhs-veryold", "vhs-dub", "vhs-baddeck", "vhs-damaged" }
local preset_name = nil
local preset_count = 0

for _, name in ipairs(preset_order) do
    if riteg.has_option(name) then
        preset_name = name
        preset_count = preset_count + 1
    end
end

if preset_count > 1 then
    riteg.log_warning("multiple VHS presets specified; using -" .. preset_name)
end

local preset = preset_name and presets[preset_name] or nil

local function preset_value(key, fallback)
    if preset and preset[key] ~= nil then
        return preset[key]
    end

    return fallback
end

-- One run is one tape played on one deck. -seed makes the pair reproducible,
-- -tape and -deck override a single side of it. Wear is derived from the tape
-- id by default, so two different tapes are worn differently without any
-- extra arguments.
local base_seed = math.floor(option("seed", math.random(0, 65535), 0, 65535))
local tape_seed = math.floor(option("tape", base_seed, 0, 65535))
local deck_seed = math.floor(option("deck", (base_seed * 1664525 + 1013904223) % 65536, 0, 65535))

-- Stable per-deck pseudo-random value. Used only for defaults; every one of
-- these is overridable, so a deck can be described by a seed or by hand.
local function deck_rand(salt)
    return ((deck_seed * 1103515245 + salt * 12345) % 10000) / 10000.0
end

local function tape_rand(salt)
    return ((tape_seed * 3571 + salt * 1237) % 10000) / 10000.0
end

-- Tape condition. This is the only thing that drives oxide loss, dropouts
-- and scratches. Wear is a threshold on the tape field, not a screen overlay.
-- Defaults are a nearly new tape: enough wear for a slightly grainy, slightly
-- soft picture, but below the oxide/dropout thresholds. Wear it down with
-- -wear 1.0 and up.
local tape_wear = option("wear", preset_value("wear", 0.05 + 0.03 * (tape_seed % 5) / 4.0), 0.0, 2.0)
local tape_dropout = option("dropout", preset_value("dropout", 0.003 + 0.002 * (tape_seed % 5) / 4.0), 0.0, 0.2)
local tape_grain = option("grain", preset_value("grain", 0.018 + 0.006 * (tape_seed % 5) / 4.0), 0.0, 0.3)
local tape_scratch = option("scratch", preset_value("scratch", 0.0), 0.0, 1.0)
local tape_crease = option("crease", preset_value("crease", 0.0), 0.0, 1.0)
local tape_doc = option("doc", preset_value("doc", 1.0), 0.0, 1.0)
local tape_sparkle = option("sparkle", preset_value("sparkle", 0.8), 0.0, 1.0)
local tape_chroma_phase = option("chroma-phase", preset_value("chroma_phase", 0.02 + 0.01 * (tape_seed % 5) / 4.0), 0.0, 1.0)
local tape_wobble = option("wobble", preset_value("wobble", 0.005 + 0.003 * (tape_seed % 5) / 4.0), 0.0, 0.1)

-- Tape geometry. A track holds one field (240 active lines), a frame is two
-- tracks. `band` is the along-track height of the coarsest wear patch in field
-- lines; `tape-life` is its across-track length in tracks. They are
-- deliberately separate: a tall patch can be short-lived and vice versa.
local tape_band = option("band", preset_value("band", 48.0 + 16.0 * tape_rand(11)), 4.0, 160.0)
local tape_life = option("tape-life", preset_value("life", 96.0 + 64.0 * tape_rand(12)), 4.0, 512.0)
local tape_position = option("position", preset_value("position", 0.0), 0.0, 65535.0)

-- Deck condition. Head switch, wow/flutter, frame lock and TBC are the deck's,
-- not the tape's; only the head-to-tape contact is shared. Defaults are a
-- healthy consumer deck: TBC on, frame lock held, only a thin head-switch
-- strip at the bottom.
local deck_headswitch = option("headswitch", preset_value("headswitch", 0.0045 + 0.0025 * deck_rand(1)), 0.002, 0.04)
local deck_wow = option("wow", preset_value("wow", 0.0001 + 0.0002 * deck_rand(2)), 0.0, 0.01)
local deck_flutter = option("flutter", preset_value("flutter", 0.00005 + 0.00008 * deck_rand(3)), 0.0, 0.005)
local deck_framelock = option("framelock", preset_value("framelock", 0.985 + 0.013 * deck_rand(4)), 0.0, 1.0)
local deck_tbc = option("tbc", preset_value("tbc", 0.94 + 0.05 * deck_rand(5)), 0.0, 1.0)
local deck_noise = option("noise", preset_value("noise", 0.55 + 0.25 * deck_rand(6)), 0.0, 2.0)
local deck_tracking = option("tracking", preset_value("tracking", 0.0), 0.0, 1.0)
local deck_frame_bounce = option("frame-bounce", preset_value("frame_bounce", 0.004), 0.0, 0.1)
local deck_frame_step = option("frame-step", preset_value("frame_step", 2.0), 1.0, 12.0)
local deck_track_phase = deck_rand(13)

-- How many record/playback passes the picture makes. 1 is one deck, 2 is a
-- single dub. Every pass applies the VHS record bandwidth again, so softness,
-- noise and chroma phase error accumulate like they do on real copies. A new
-- tape on a working deck is a single pass.
local generations = math.floor(option("generations", preset_value("generations", 1), 1, 4))

local tape_slant <const> = 2.0 / base_tall
local tape_breath = option("breath", preset_value("breath", 0.02 + 0.01 * tape_rand(13)), 0.0, 0.1)
local flag_amount = option("flag", preset_value("flag", 0.0), 0.0, 1.0)

-- VHS composite timing. One active line is 227.5 colorburst cycles, and the
-- encode runs at 2 * base_wide, so the wavelength has to follow the target
-- width instead of being a magic 3.0 tuned for 1280.
local encode_wide <const> = 2.0 * base_wide
local colorburst_period <const> = encode_wide / 227.5
local colorburst_wavelength <const> = colorburst_period / 2.0
local am_wavelength <const> = math.max(2.0, colorburst_wavelength / 2.0)

-- About 3 MHz of luma: 3e6 / 15734 = 190.7 cycles per line.
local luma_wavelength <const> = encode_wide / (2.0 * (3.0e6 / 15734.0))

local output_level <const> = 0.98 ^ generations

-- The empirical matrix was calibrated for two generations. If the flat-colour
-- transfer is roughly linear and the same on every pass, the inverse for N
-- passes is the two-generation inverse raised to N/2. These are the matrix
-- powers C^(N/2), flattened for GLSL column-major mat3.
local color_corrections = {
    [1] = "mat3(0.950869012, -0.002758537, 0.119277997, -0.020392899, 1.045889499, -0.176065116, 0.068898331, -0.043055253, 1.050596230)",
    [2] = "mat3(0.912426188, -0.010643677, 0.239216447, -0.052850288, 1.101521626, -0.371550428, 0.138775636, -0.090454783, 1.119551022)",
    [3] = "mat3(0.884296458, -0.023948596, 0.362026246, -0.098316125, 1.168212888, -0.590592889, 0.210937284, -0.143191078, 1.208674895)",
    [4] = "mat3(0.866281485, -0.043074082, 0.490037031, -0.157999869, 1.247520927, -0.837883151, 0.286769491, -0.202383727, 1.320200419)",
}
local color_correction = color_corrections[generations] or color_corrections[2]

riteg.log_info(string.format(
    "VHS hop: preset=%s tape=%d deck=%d gen=%d wear=%.3f dropout=%.4f grain=%.4f band=%.2f life=%.2f pos=%.1f tracking=%.3f headswitch=%.4f wow=%.5f flutter=%.5f framelock=%.3f tbc=%.3f noise=%.3f crease=%.3f flag=%.3f",
    preset_name or "new(default)", tape_seed, deck_seed, generations, tape_wear, tape_dropout, tape_grain, tape_band, tape_life, tape_position,
    deck_tracking, deck_headswitch, deck_wow, deck_flutter, deck_framelock, deck_tbc, deck_noise, tape_crease, flag_amount))

-- Define helpers keep the big shader string free of a 40-argument format call.
local function define(name, value)
    return string.format("#define %s (%s)\n", name, value)
end

local defines = table.concat({
    "#ifndef GEN_ENTROPY\n#define GEN_ENTROPY (0U)\n#endif\n",
    define("TAPE_SEED", string.format("%dU", tape_seed)),
    define("DECK_SEED", string.format("%dU", deck_seed)),
    define("TAPE_WEAR", string.format("%.6f", tape_wear)),
    define("TAPE_DROPOUT", string.format("%.6f", tape_dropout)),
    define("TAPE_GRAIN", string.format("%.6f", tape_grain)),
    define("TAPE_BREATH", string.format("%.6f", tape_breath)),
    define("TAPE_CHROMA_PHASE", string.format("%.6f", tape_chroma_phase)),
    define("TAPE_DOC", string.format("%.6f", tape_doc)),
    define("TAPE_SPARKLE", string.format("%.6f", tape_sparkle)),
    define("TAPE_SCRATCH", string.format("%.6f", tape_scratch)),
    define("TAPE_CREASE", string.format("%.6f", tape_crease)),
    define("TAPE_FLAG", string.format("%.6f", flag_amount)),
    define("TAPE_WOBBLE", string.format("%.6f", tape_wobble)),
    define("TAPE_SLANT", string.format("%.6f", tape_slant)),
    define("TAPE_ROWS", "480.0"),
    define("TAPE_ROWS_PER_FIELD", "240.0"),
    define("TAPE_TRACKS_PER_FRAME", "2.0"),
    define("TAPE_BAND_ROWS", string.format("%.4f", tape_band)),
    define("TAPE_LIFE_TRACKS", string.format("%.4f", tape_life)),
    define("TAPE_POSITION", string.format("%.4f", tape_position)),
    define("TAPE_FLOOR", "0.01"),
    define("TAPE_DROPOUT_BASE", "0.0002"),
    define("TAPE_DROPOUT_WEAR", "0.02"),
    define("TAPE_DOC_NOISE", "0.15"),
    define("TAPE_TRACKING_WIDTH", "0.035"),
    define("TAPE_TRACKING_SCROLL", string.format("%.7f", 1.0 / 180.0)),
    define("TAPE_CREASE_ANGLE", string.format("%.5f", 0.10 + 0.15 * tape_rand(8))),
    define("TAPE_CREASE_POS", string.format("%.5f", 0.25 + 0.5 * tape_rand(9))),
    define("TAPE_CREASE_SHIFT", "0.012"),
    define("DECK_HEADSWITCH", string.format("%.6f", deck_headswitch)),
    define("DECK_WOW", string.format("%.6f", deck_wow)),
    define("DECK_FLUTTER", string.format("%.6f", deck_flutter)),
    define("DECK_FRAMELOCK", string.format("%.6f", deck_framelock)),
    define("DECK_FRAME_BOUNCE", string.format("%.6f", deck_frame_bounce)),
    define("DECK_FRAME_STEP", string.format("%.3f", deck_frame_step)),
    define("DECK_TBC", string.format("%.6f", deck_tbc)),
    define("DECK_NOISE", string.format("%.6f", deck_noise)),
    define("DECK_TRACKING", string.format("%.6f", deck_tracking)),
    define("DECK_TRACK_PHASE", string.format("%.6f", deck_track_phase)),
    define("DECODE_WINDOW_WIDTH", "16"),
    define("ENCODE_WINDOW_WIDTH", "8"),
    define("AM_CARRIERSIGNAL_WAVELENGTH", string.format("%.6f", am_wavelength)),
    define("AM_DECODE_HIGHPASS_WAVELENGTH", string.format("%.6f", am_wavelength)),
    define("AM_DEMODULATE_WAVELENGTH", string.format("%.6f", am_wavelength)),
    define("COLORBURST_WAVELENGTH_ENCODER", string.format("%.6f", colorburst_wavelength)),
    define("COLORBURST_WAVELENGTH_DECODER", string.format("%.6f", colorburst_wavelength)),
    define("YLOWPASS_WAVELENGTH", string.format("%.6f", luma_wavelength)),
    define("ILOWPASS_WAVELENGTH", "8.0"),
    define("QLOWPASS_WAVELENGTH", "11.0"),
    define("DECODE_LOWPASS_WAVELENGTH", string.format("%.6f", luma_wavelength)),
    define("NTSC_SCALE", "1.0"),
    define("NOISE_STRENGTH", "0.015625"),
    define("SATURATION", "4.0"),
    define("WINDOW_BIAS", "0.0"),
    define("VHS_DROPOUT_BLUR_STEPS", "64"),
    define("VHS_DROPOUT_MIN_STEPS", "32"),
    define("VHS_MAXRES_Y", "vec2(333.0, 480.0)"),
    define("VHS_MAXRES_IQ", "vec2(40.0, 480.0)"),
    define("VHS_BLUR_AMT", "vec2(0.2, 0.2)"),
    -- Artistic contrast control, not the VHS/CRT transfer: the signal is
    -- expanded before the composite chain and compressed at the end.
    define("GAMMA_CORRECTION", "0.707"),
    define("OUTPUT_LEVEL", string.format("%.6f", output_level)),
    define("RGB_TO_YIQ", "mat3(0.299, 0.587, 0.114, 0.595, -0.274, -0.3213, 0.2115, -0.5227, 0.3112)"),
    define("YIQ_TO_RGB", "mat3(1.0, 0.956, 0.619, 1.0, -0.272, -0.647, 1.0, -1.106, 1.703)"),
    -- Empirical inverse of the flat-colour transfer at SATURATION = 4,
    -- selected for the current generation count. Recompute the table if the
    -- NTSC constants change.
    define("COLOR_CORRECTION", color_correction),
}, "")

-- A bunch of GLSL code that is shared between all the
-- passes in the filter pipeline; this contains bits from
-- both VHS compression, NTSC filter and my own code
local common_glsl <const> = defines .. [[
    const float PI = 3.14159265359;

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
        return uint(uint(uv.x) * 1973U + uint(uv.y) * 9277U + uint(iFrame) * 26699U
            + entropy * 34961U + GEN_ENTROPY * 104729U + DECK_SEED * 41077U) | 1U;
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

    // ---- Tape coordinates -------------------------------------------------
    // The frame is 480 lines, but the tape is written one field per track.
    // Field line = floor(frameLine / 2); the two fields are interleaved and
    // their tracks differ, so a defect can be stronger in one field than in
    // the other. `track` advances by one per field and by two per frame.

    float Tape_line(vec2 uv)
    {
        return floor(uv.y * TAPE_ROWS);
    }

    float Tape_field(vec2 uv)
    {
        return mod(Tape_line(uv), 2.0);
    }

    float Tape_track(vec2 uv)
    {
        return iFrame * TAPE_TRACKS_PER_FRAME + Tape_field(uv) + TAPE_POSITION;
    }

    // 0..1 along one field track. A constant value is a horizontal band.
    float Tape_along(vec2 uv)
    {
        return (floor(uv.y * TAPE_ROWS_PER_FIELD) + 0.5) / TAPE_ROWS_PER_FIELD;
    }

    // Wear as a function of the tape, not of the frame. Each octave has its
    // own height in field lines (TAPE_BAND_ROWS) and its own length in tracks
    // (TAPE_LIFE_TRACKS), so height and lifetime are not locked together.
    float Tape_wear(vec2 uv)
    {
        float along = Tape_along(uv) + TAPE_SLANT * uv.x;
        float track = Tape_track(uv);

        float n = 0.55 * Tape_noise(vec2(along * (TAPE_ROWS_PER_FIELD / TAPE_BAND_ROWS), track / TAPE_LIFE_TRACKS), 20U)
                + 0.30 * Tape_noise(vec2(along * (TAPE_ROWS_PER_FIELD / (TAPE_BAND_ROWS * 0.25)), track / (TAPE_LIFE_TRACKS * 0.25)), 21U)
                + 0.15 * Tape_noise(vec2(along * (TAPE_ROWS_PER_FIELD / (TAPE_BAND_ROWS * 0.0625)), track / (TAPE_LIFE_TRACKS * 0.0625)), 22U);

        float floorWear = TAPE_FLOOR * step(0.005, TAPE_WEAR);
        return clamp(TAPE_WEAR * smoothstep(0.52, 0.76, n) + floorWear, 0.0, 1.0);
    }

    // A crease is a diagonal fold: a thin line with a lateral displacement.
    // It is static on the tape, so it is derived from the tape id only.
    float Tape_crease(vec2 uv)
    {
        float d = uv.y - (TAPE_CREASE_POS + TAPE_CREASE_ANGLE * uv.x);
        float line = smoothstep(0.0035, 0.0, abs(d));
        return line * clamp(TAPE_CREASE, 0.0, 1.0);
    }

    // Time-base error: deck wow/flutter (periodic) plus tape scrape (noise),
    // plus a crease step, all reduced by the deck's TBC. Stored as a fraction
    // of the frame width in the mask and applied to the input sampling.
    float Tape_shift(vec2 uv, float wear)
    {
        float along = Tape_along(uv);
        float track = Tape_track(uv);
        float t = float(iFrame) / 30.0;

        float wow = sin(t * 6.2831853 * 0.73 + TAPE_POSITION * 0.37) * DECK_WOW;
        float flutter = sin(t * 6.2831853 * 6.1 + along * 47.0) * DECK_FLUTTER;
        float scrape = (2.0 * Tape_noise(vec2(along * 120.0, track / 6.0), 30U) - 1.0) * TAPE_WOBBLE * wear;
        float crease = Tape_crease(uv) * TAPE_CREASE_SHIFT;

        return (wow + flutter + scrape + crease) * (1.0 - DECK_TBC);
    }

    // Whole-frame vertical sync offset, in frame fractions. Only a deck that
    // is not frame-locked can slip; the offset snaps in whole lines because
    // the field phase does.
    float Tape_frameOffset()
    {
        float slow = 2.0 * Tape_noise(vec2(float(iFrame) / 72.0, 0.5), 70U) - 1.0;
        float fast = 2.0 * Tape_noise(vec2(float(iFrame) / 12.0, 0.5), 71U) - 1.0;
        float amount = (1.0 - DECK_FRAMELOCK) * DECK_FRAME_BOUNCE;
        float bounce = (0.6 * slow + 0.4 * fast) * amount;
        return floor(bounce * TAPE_ROWS / DECK_FRAME_STEP + 0.5) * DECK_FRAME_STEP / TAPE_ROWS;
    }

    // Head-to-tape contact breathes over seconds: the whole frame slowly
    // gains and loses level. One slow noise, so it is global and smooth.
    float Tape_breath()
    {
        float n = Tape_noise(vec2(float(iFrame) / 72.0, 0.5), 60U);
        return 2.0 * n - 1.0;
    }

    // A scratch is a longitudinal line. It is permanent in tape coordinates;
    // only its depth varies with the track that is being read. The row is a
    // field line, so it spans two adjacent frame rows (one per field).
    float Tape_scratch(vec2 uv)
    {
        float row = floor(uv.y * TAPE_ROWS_PER_FIELD);
        float track = Tape_track(uv);

        uint posState = Common_tapeRng(vec2(0.0, row), 50U);
        float pos = Common_randomFloat(posState);
        float rate = 0.02 * TAPE_SCRATCH * clamp(TAPE_WEAR, 0.0, 1.0);
        float canScratch = step(1.0 - rate, pos);

        float severity = Tape_noise(vec2(row * 0.7, track / max(4.0, TAPE_LIFE_TRACKS * 0.25)), 51U);
        return canScratch * smoothstep(0.35, 0.65, severity);
    }

    // A dropout is a hole in the oxide. The hole map lives in tape
    // coordinates: fixed along the line and correlated across a few tracks,
    // so the same hole is read again for as long as its tracks come around.
    // The snow inside it is fresh every frame, but the hole itself is not.
    float Tape_holes(vec2 uv)
    {
        vec2 cell = vec2(floor(uv.x * 256.0), floor(uv.y * TAPE_ROWS_PER_FIELD));
        float track = Tape_track(uv);
        return Tape_noise(vec2(cell.x + cell.y * 257.0, track / 4.0), 90U);
    }

    // A mistracked deck reads the guard band instead of the track: a wide
    // horizontal band of noise and lost colour that scrolls through the
    // picture. 0 is perfect tracking, 1 is a completely lost servo.
    float Tape_trackingBand(vec2 uv)
    {
        float amount = clamp(DECK_TRACKING, 0.0, 1.0);
        if(amount <= 0.0)
            return 0.0;

        float y = fract(DECK_TRACK_PHASE + float(iFrame) * TAPE_TRACKING_SCROLL);
        float d = abs(uv.y - y);
        d = min(d, 1.0 - d);
        return smoothstep(TAPE_TRACKING_WIDTH, 0.0, d) * amount;
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

    float NTSC_fullEncode(sampler2D sampler, in vec2 uv, in float pixelWidth)
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
        float phaseAM = uv.x * PI / (AM_CARRIERSIGNAL_WAVELENGTH * pixelWidth);
        return sin(phaseAM) * (yiq.x + yiq.y * sin(phase) + yiq.z * cos(phase));
    }

    float NTSC_decodeAM(sampler2D sampler, in vec2 uv, in float pixelWidth)
    {
        float phaseAM = uv.x * PI / (AM_DEMODULATE_WAVELENGTH * pixelWidth);
        float decoded = 0.0;

        for(int i = -DECODE_WINDOW_WIDTH; i <= DECODE_WINDOW_WIDTH; ++i) {
            float window = NTSC_cosineWindow(float(i) / float(DECODE_WINDOW_WIDTH + 1));

            vec2 uvWithOffset = vec2(uv.x + float(i) * pixelWidth, uv.y);

            float sinc = NTSC_sinc(float(i) / AM_DECODE_HIGHPASS_WAVELENGTH) / AM_DECODE_HIGHPASS_WAVELENGTH;
            float encodedSample = texture(sampler, uvWithOffset).x;

            decoded += encodedSample * sinc * window;
        }

        return decoded * sin(phaseAM) * 4.0;
    }

    vec3 NTSC_fullDecodeYIQ(sampler2D sampler, in vec2 uv, in float pixelWidth, uint rngRow, float tapeWear)
    {
        uint seed = rngRow;
        vec2 originalUV = uv;

        // Per-line chroma phase error. The encoder and decoder now use the
        // same reference phase, so this is a real error rather than the old
        // per-line PI that cancelled itself out.
        float phaseNoise = Common_randomFloat(seed) * 2.0 - 1.0;
        phaseNoise *= TAPE_CHROMA_PHASE * (0.15 + 0.85 * clamp(tapeWear, 0.0, 1.0));
        phaseNoise += (Common_randomFloat(seed) * 2.0 - 1.0) * 0.004 * DECK_NOISE;

        float frequencyNoise = Common_randomFloat(seed) * 2.0 - 1.0;
        frequencyNoise *= TAPE_CHROMA_PHASE * 0.02 * clamp(tapeWear, 0.0, 1.0);

        vec3 yiq = vec3(0.0);
        float windowWeight = 0.0;

        for(int i = -DECODE_WINDOW_WIDTH; i <= DECODE_WINDOW_WIDTH; ++i) {
            float window = NTSC_cosineWindow(float(i) / float(DECODE_WINDOW_WIDTH + 1));
            vec2 uvWithOffset = vec2(uv.x + float(i) * pixelWidth, uv.y);
            vec2 originalUVWithOffset = vec2(originalUV.x + float(i) * pixelWidth, originalUV.y);
            float phase = originalUVWithOffset.x * PI / ((COLORBURST_WAVELENGTH_DECODER + frequencyNoise) * pixelWidth) + phaseNoise;

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
]]

-- The image is an interlaced source, not a progressive frame. Each field has
-- 240 lines; a field line integrates the two source rows around it and the
-- second field is offset by half a line. On a still this mostly costs vertical
-- detail and creates interlace aliasing; on a sequence it gives the two fields
-- the right line phase (temporal field differences are outside this graph).
local source_field_src <const> = common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord / iResolution.xy;
        float field = mod(floor(uv.y * TAPE_ROWS), 2.0);
        float line = floor(uv.y * TAPE_ROWS_PER_FIELD);
        float v = (line + 0.5 + 0.5 * field) / TAPE_ROWS_PER_FIELD;

        vec4 a = texture(iChannel0, vec2(uv.x, v - 0.25 / TAPE_ROWS_PER_FIELD));
        vec4 b = texture(iChannel0, vec2(uv.x, v + 0.25 / TAPE_ROWS_PER_FIELD));
        fragColor = 0.5 * (a + b);
        fragColor.a = 1.0;
    }
]]

local gamma_correct_src <const> = common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord / iResolution.xy;
        float wear = texture(iChannel1, uv).x;
        float shift = texture(iChannel1, uv).y;

        // Whole-frame sync drift: the picture slides and wraps. The wrapped
        // rows are marked as dead reads in the dropout pass.
        float bounce = Tape_frameOffset();
        vec4 color = texture(iChannel0, vec2(uv.x + shift, fract(uv.y + bounce)));
        fragColor.rgb = pow(color.rgb, vec3(1.0 / GAMMA_CORRECTION));
        fragColor.a = color.a;
    }
]]

-- The record-side bandwidth limit. This is where VHS loses detail: the tape
-- cannot carry more than about 3 MHz of luma and a fraction of a megahertz of
-- chroma. Applying it before the composite encoder means the cross-colour and
-- ringing are generated inside the VHS band, not at the full 2x width and then
-- blurred away. The playback limit is the 333/40 downsample later.
local pre_band_src <const> = common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord / iResolution.xy;
        vec2 px = 1.0 / iResolution.xy;

        float y = 0.0;
        float wySum = 0.0;
        for (int i = -8; i <= 8; ++i) {
            float x = float(i);
            float wy = exp(-0.5 * (x / 2.0) * (x / 2.0));
            vec3 s = texture(iChannel0, uv + vec2(x * px.x, 0.0)).rgb * RGB_TO_YIQ;
            y += s.x * wy;
            wySum += wy;
        }
        y /= wySum;

        vec2 iq = vec2(0.0);
        float wiSum = 0.0;
        for (int i = -16; i <= 16; ++i) {
            float x = float(i) * 2.0;
            float wi = exp(-0.5 * (x / 10.0) * (x / 10.0));
            vec3 s = texture(iChannel0, uv + vec2(x * px.x, 0.0)).rgb * RGB_TO_YIQ;
            iq += s.yz * wi;
            wiSum += wi;
        }
        iq /= wiSum;

        fragColor = vec4(vec3(y, iq) * YIQ_TO_RGB, 1.0);
    }
]]

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
-- only place the wear mask enters the signal chain: every generation is a
-- fresh recording, so every generation carries its own tape noise. The mask
-- stays a threshold on the same tape field, so a bad band gets denser specks,
-- not a colored patch.
local function ntsc_encode_src(gen, tape_gain)
    local wear_decl = "float wear = 0.0;"
    if tape_gain > 0.0 then
        wear_decl = "float wear = texture(iChannel1, uv).x;"
    end

    return string.format("#define GEN_ENTROPY (%dU)\n", gen) .. common_glsl .. string.format([[
        void mainImage(out vec4 fragColor, in vec2 fragCoord)
        {
            uint rngState = Common_newRngStateEx(fragCoord, 1U);
            uint rngStateRow = Common_newRngStateEx(vec2(0.0, fragCoord.y), 2U);

            vec2 uv = fragCoord / iResolution.xy;
            vec2 pixelSize = 1.0 / iResolution.xy;
            %s

            float encoded = NTSC_fullEncode(iChannel0, uv, pixelSize.x * NTSC_SCALE);

            float snowNoise = Common_randomFloat(rngState) - 0.5;
            float sineNoise = sin(uv.x * 200.0 + uv.y * -50.0 + fract(iTime * iTime) * 3.1415927 * 2.0) * 0.065;
            float saltPepperNoise = Common_randomFloat(rngState) * 2.0 - 1.0;

            saltPepperNoise = sign(saltPepperNoise) * pow(abs(saltPepperNoise), 200.0) * 10.0;

            float rowNoise = Common_randomFloat(rngStateRow) * 2.0 - 1.0;
            rowNoise *= 0.1;

            float rowSaltPepper = Common_randomFloat(rngStateRow) * 2.0 - 1.0;
            rowSaltPepper = sign(rowSaltPepper) * pow(abs(rowSaltPepper), 200.0) * 1.0;

            float noise = snowNoise + saltPepperNoise + sineNoise + rowNoise + rowSaltPepper;
            encoded += noise * NOISE_STRENGTH * (1.0 + %f * wear) * DECK_NOISE;

            fragColor.xyz = vec3(encoded);
            fragColor.w = 1.0;
        }
    ]], wear_decl, tape_gain)
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

local function ntsc_decode_iq_src(gen)
    return string.format("#define GEN_ENTROPY (%dU)\n", gen) .. common_glsl .. [[
        void mainImage(out vec4 fragColor, in vec2 fragCoord)
        {
            vec2 uv = fragCoord / iResolution.xy;
            vec2 pixelSize = 1.0 / iResolution.xy;
            float wear = texture(iChannel1, uv).x;

            uint rngStateRow = Common_newRngState(vec2(0.0, fragCoord.y));

            fragColor.xyz = NTSC_fullDecodeYIQ(iChannel0, uv, pixelSize.x * NTSC_SCALE, rngStateRow, wear);
            fragColor.w = 1.0;
        }
    ]]
end

-- Repack the final YIQ into the two VHS playback bandwidths. The downsample
-- also does the 2:1 decimation from the 2x composite width to the base width.
local vhs_downsample_src <const> = common_glsl .. [[
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
]]

local vhs_upsample_src <const> = common_glsl .. [[
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
]]

-- The mask is the only place the tape field is evaluated. R is wear, G is the
-- horizontal time-base error of that line (deck + tape + crease). It is one
-- sample per field line (480 rows, both fields) and 32 columns across, because
-- the tape pattern varies along the track and the frame is only wide because
-- the raster is. Consumers sample it at their own uv.
local tape_mask = make_pass(common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord / iResolution.xy;
        float wear = Tape_wear(uv);
        fragColor = vec4(wear, Tape_shift(uv, wear), 0.0, 1.0);
    }
]], 32, base_tall, nil, { format = "rg16f", filter = "linear" })

-- Dropouts are a loss of signal over a finite time, i.e. over a run of
-- pixels along the raster, not over one pixel. The hole map is in tape
-- coordinates, so a hole recurs while its tracks are being read; only the
-- snow that fills it is new every frame. `Tape_scratch` and the tracking band
-- raise the threshold at their lines instead of growing a second generator.
local vhs_dropout = make_pass(common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord.xy / iResolution.xy;
        float wear = texture(iChannel0, uv).x;

        vec2 cell = vec2(floor(uv.x * 256.0), floor(uv.y * TAPE_ROWS_PER_FIELD));

        float density = TAPE_DROPOUT * smoothstep(0.25, 0.80, wear) * (0.4 + 0.6 * (1.0 - uv.y));
        float baseDropout = TAPE_DROPOUT * (TAPE_DROPOUT_BASE
            + TAPE_DROPOUT_WEAR * smoothstep(0.30, 1.0, TAPE_WEAR))
            * step(0.005, TAPE_WEAR) * DECK_NOISE;
        density = max(density, baseDropout);
        density = max(density, Tape_scratch(uv));
        density = max(density, Tape_trackingBand(uv) * 0.6);

        // Rows where the frame offset wrapped show the next/previous track's
        // stub, read from the side. Treat the whole band as one dead read.
        float frameY = uv.y + Tape_frameOffset();
        density = max(density, step(1.0, frameY) + step(frameY, 0.0));

        float hole = Tape_holes(uv);
        fragColor.x = step(1.0 - clamp(density, 0.0, 1.0), hole);

        uint rngI = Common_newRngState(cell + vec2(17.0, 0.0));
        uint rngQ = Common_newRngState(cell + vec2(43.0, 0.0));
        fragColor.y = 2.0 * Common_randomFloat(rngI) - 1.0;
        fragColor.z = 2.0 * Common_randomFloat(rngQ) - 1.0;
        fragColor.w = 1.0;
    }
]], base_wide, base_tall, { [0] = tape_mask })

-- The length is a property of the hole, not of each pixel, so it comes from a
-- coarse tape coordinate. The accumulation is a horizontal smear; a real DOC
-- would hold one whole line, but the visible result here is the same kind of
-- finite run rather than a single white pixel.
local vhs_dropout_blur = make_pass(common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord.xy / iResolution.xy;
        vec2 pixel = 1.0 / iResolution.xy;
        float wear = texture(iChannel1, uv).x;

        float run = floor(uv.x * 16.0);
        float lenRand = Tape_noise(vec2(run * 0.15, Tape_track(uv) / 8.0), 80U);

        fragColor = vec4(0.0, 0.0, 0.0, 1.0);

        int steps = int(max(float(VHS_DROPOUT_MIN_STEPS),
            VHS_DROPOUT_BLUR_STEPS * (0.2 + 0.8 * wear) * lenRand * DECK_NOISE));

        for(int i = 0; i < steps; ++i) {
            vec2 uvmod = vec2(uv.x - pixel.x * float(i), uv.y);
            vec4 sample = texture(iChannel0, uvmod);
            fragColor += sample / (0.125 * float(i + 1));
        }

        fragColor.x = clamp(fragColor.x, 0.0, 1.0);
        fragColor.yz = clamp(fragColor.yz, -1.0, 1.0);
    }
]], base_wide, base_tall, { [0] = vhs_dropout, [1] = tape_mask })

-- Pipeline: field sampling -> head read (TBE, frame slip) -> per-generation
-- record bandwidth -> composite encode/decode -> playback bandwidth ->
-- baseband tape artifacts -> display.
local source_field = make_pass(source_field_src, base_wide, base_tall, { [0] = riteg.get_image_source() })
local gamma_correct = make_pass(gamma_correct_src, base_wide, base_tall, { [0] = source_field, [1] = tape_mask })

local generation_input = gamma_correct
local generation_yiq = nil

for generation = 1, generations do
    if generation > 1 then
        generation_input = make_pass(yiq_to_rgb_src, base_wide, base_tall, { [0] = generation_yiq })
    end

    local pre_band = make_pass(pre_band_src, base_wide, base_tall, { [0] = generation_input })
    local encode = make_pass(ntsc_encode_src(generation, 8.0), 2.0 * base_wide, base_tall, { [0] = pre_band }, { format = "r16f" })
    local decode_y = make_pass(ntsc_decode_y_src, 2.0 * base_wide, base_tall, { [0] = encode }, { format = "r16f" })
    generation_yiq = make_pass(ntsc_decode_iq_src(generation), 2.0 * base_wide, base_tall, { [0] = decode_y, [1] = tape_mask })
end

-- Playback bandwidth: the deck cannot reproduce more than the tape carries.
local playback_down = make_pass(vhs_downsample_src, downsample_wide, base_tall, { [0] = generation_yiq })
local vhs_upsample = make_pass(vhs_upsample_src, base_wide, base_tall, { [0] = playback_down })

-- The baseband side of wear. Oxide loss first: the band loses fine detail,
-- level and chroma, and gets its own chroma noise (VHS colour dies first).
-- Then the dropout layer: the compensator holds the previous field line, but
-- a dropout is lost signal, so luma and chroma become coloured snow. A
-- scratch is the same loss for a whole row, at full strength. The tracking
-- band and a crease are read errors and add their own noise here.
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
        float grain = (Common_randomFloat(rngState) - 0.5) * TAPE_GRAIN * (0.15 + 0.85 * wear) * DECK_NOISE;

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

        // What the one-field-line delay holds. The raster runs from the top
        // of the picture down (uv.y = 1 -> 0), and one field line is two frame
        // rows, so the previous line is +2 rows, not -1.
        vec4 prev = texture(iChannel0, uv + vec2(0.0, 2.0 * pixel.y));

        // The substituted line is noisy too, otherwise the hold looks clean.
        vec3 held = prev.xyz + vec3(n1, n2, n3) * TAPE_DOC_NOISE;

        float mask = clamp(dropout.x, 0.0, 1.0);
        vec3 result = mix(base, held, mask * TAPE_DOC);

        // A dropout is a loss of signal, so it reads as coloured snow, not
        // as one flat white bar: luma and chroma are both noise here.
        float scratch = Tape_scratch(uv);
        float streak = mask * (1.0 - scratch);

        // The wrapped band is a dead read, but its noise should follow the
        // signal level: on black it stays dark instead of flashing white.
        float frameY = uv.y + Tape_frameOffset();
        float band = step(1.0, frameY) + step(frameY, 0.0);
        float level = clamp(result.x, 0.0, 1.0);
        float bandDead = mix(1.0, 0.10 + 0.90 * level, band);

        float dead = clamp(streak * TAPE_SPARKLE, 0.0, 1.0) * bandDead;
        result.x = mix(result.x, snow, dead);
        result.yz = mix(result.yz, vec2(n2, n3) * 1.2, streak * (1.0 - 0.90 * band));

        // A scratch takes the whole row down, so it is snow at full strength.
        result.x = mix(result.x, snow, scratch);
        result.yz = mix(result.yz, vec2(n2, n3) * 0.12, scratch);

        // Mistracking and a crease are read errors, not oxide loss: they
        // take the level and the colour out of their line/band.
        float tracking = Tape_trackingBand(uv);
        result.x = mix(result.x, snow, tracking * 0.8);
        result.yz = mix(result.yz, vec2(n2, n3) * 0.5, tracking);
        result.x *= 1.0 - 0.4 * tracking;

        float crease = Tape_crease(uv);
        result.x = mix(result.x, snow, crease * 0.5);
        result.x *= 1.0 - 0.3 * crease;

        // Contact breathing: a slow global level wobble that also lifts the
        // grain while the head is pressed harder against the tape.
        float breath = Tape_breath() * TAPE_BREATH;
        result.x = result.x * (1.0 + breath) + grain * (1.0 + 2.0 * breath);

        fragColor = vec4(result, 1.0);
    }
]], base_wide, base_tall, { [0] = vhs_upsample, [1] = vhs_dropout_blur, [2] = tape_mask })

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

        // The head-switch zone is a property of the deck, not of the tape.
        // Wear only makes the switch noisier, it does not widen the zone.
        float zone = DECK_HEADSWITCH;
        float wearJiggle = 1.0 + 2.0 * clamp(TAPE_WEAR, 0.0, 1.0);

        if(uv.y <= zone) {
            float amount = 1.0 / exp((zone - uv.y) * 10.0 * P1 + P2);
            float kjig = 1.0 / exp((zone - uv.y) * P1 + P2 - 0.5);

            float jiggle = P3 * kjig * Common_randomFloat(rngStateRow) * DECK_NOISE * wearJiggle;
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

-- Flagging: a deck with a weak servo tears the top of the picture. The tear is
-- horizontal, a few lines deep, and grows as frame lock and tracking get worse.
local vhs_flagging = make_pass(common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord.xy / iResolution.xy;
        float amount = TAPE_FLAG * (0.3 + 0.7 * DECK_TRACKING) * (1.0 - DECK_FRAMELOCK);
        float zone = 0.015 + 0.05 * amount;

        if(amount > 0.0 && uv.y > 1.0 - zone) {
            float t = (uv.y - (1.0 - zone)) / zone;
            uint s = Common_newRngState(vec2(floor(uv.y * TAPE_ROWS), 0.0));
            float shift = (2.0 * Common_randomFloat(s) - 1.0) * 0.01 * amount * t;
            uv.x = clamp(uv.x + shift, 0.0, 1.0);
        }

        fragColor = texture(iChannel0, uv);
    }
]], base_wide, base_tall, { [0] = vhs_headswitch })

-- Tracking: a mistracked head reads a scrolling band of noise. The band is
-- already used as dropout density; this pass adds the horizontal jitter and
-- the colour loss that go with it.
local vhs_tracking = make_pass(common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord / iResolution.xy;
        float band = Tape_trackingBand(uv);

        uint rngState = Common_newRngState(vec2(0.0, fragCoord.y));
        float jitter = (Common_randomFloat(rngState) * 2.0 - 1.0) * 0.004 * band;
        vec4 color = texture(iChannel0, uv + vec2(jitter, 0.0));

        color.yz *= 1.0 - 0.85 * band;
        float ci = Common_randomFloat(rngState);
        float cq = Common_randomFloat(rngState);
        color.yz += vec2(ci, cq) * 0.08 * band;

        fragColor = color;
    }
]], base_wide, base_tall, { [0] = vhs_flagging })

local final_rgb = make_pass(common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord / iResolution.xy;
        vec4 color = texture(iChannel0, uv);
        vec3 rgb = max(COLOR_CORRECTION * (color.xyz * YIQ_TO_RGB), vec3(0.0));
        fragColor.rgb = pow(rgb, vec3(GAMMA_CORRECTION)) * OUTPUT_LEVEL;
        fragColor.a = 1.0;
    }
]], out_wide, out_tall, { [0] = vhs_tracking })

riteg.output(final_rgb)
