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
-- V9 deliberately models a 480-line VHS/S-VHS signal, not a resolution-agnostic
-- effect. TAPE_ROWS is 480 and TAPE_ROWS_PER_FIELD is 240 by design; do not
-- replace them with iResolution-derived values.
--
-- Presets set the whole parameter set at once; individual options still win:
--   -vhs-new      new tape, healthy deck (the default)
--   -vhs-used     light wear, still well tracked
--   -vhs-old      visible wear bands, occasional dropouts, older deck
--   -vhs-veryold  heavy wear, creases, scratches, weak servo, two dubs
--   -vhs-dub      clean-ish tape, three record/playback generations
--   -vhs-baddeck  new tape on a deck with bad tracking/TBC/frame lock
--   -vhs-damaged  physically damaged tape: crease, scratch, dropouts, mistracking
--   -vhs-wrecked  tape that takes the signal away: killer, vertical roll, snow
--
-- Signal path options:
--   -speed sp|lp|ep       luma/chroma cutoff and playback bandwidth
--   -decode notch|comb1|comb2|2d|2d-adaptive   (default: 2d; notch = old look)
--   -headswitch N         head-switch height in lines (was a fraction)
--   -headswitch-offset N  lines of the switch hidden below the frame
--   -headswitch-shift X   horizontal shift as a fraction of width
--   -tracking-height N    bottom tracking band height in lines
--   -tracking-scroll X    strength of the old scrolling tracking band
--   -chroma-delay-h N     chroma offset in base pixels
--   -chroma-delay-v N     chroma offset in field lines
--   -chroma-blend X       vertical chroma blend with the previous field line
--   -chroma-loss X        chance that a field line loses chroma entirely
--   -rf-loss X            depth of a bad patch on the tape (0..1)
--   -killer X             how hard the color killer mutes chroma (0..1)
--   -sync-loss X          how readily the servo loses lock and rolls (0..1)
--   -roll-speed X         frame offsets per frame while unlocked (default 1/90)
--   -roll-skew X          horizontal shear per roll, fraction of width
--   -dead-density X       how dense the raster is with no RF at all (0..1)
--   -snow-level X         peak brightness of dropout/tracking snow (0..1)
--   -grain-frequency X    FBM grain frequency in cycles per base pixel
--   -grain-detail N       FBM grain octaves (1..5)
-- Example: -vhs-old -wear 0.4 -tracking 0.02 -decode comb2

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

local function option_string(name, fallback, allowed)
    local value = riteg.get_option_string(name, fallback)

    if allowed[value] then
        return value
    end

    riteg.log_warning(string.format("unknown -%s value '%s'; using '%s'", name, value, fallback))
    return fallback
end

-- VHS tape speeds. Cutoffs are in Hz, delay is in 4fsc samples.
-- S-VHS is intentionally not a separate mode here; it would extend this table
-- with a wider luma cutoff and a separate Y/C path.
local tape_speeds = {
    sp = { luma = 2.4e6, chroma = 0.32e6, delay = 4 },
    lp = { luma = 1.9e6, chroma = 0.30e6, delay = 5 },
    ep = { luma = 1.4e6, chroma = 0.28e6, delay = 6 },
}

-- Named presets. A preset only supplies defaults: every individual option
-- still wins over it, so `-vhs-old -wear 0.4` is valid. Pass exactly one
-- preset; if several are present, the most severe one in `preset_order` wins.
-- The default (no preset) is the same idea as -vhs-new, with a little
-- seed-to-seed variation in the exact wear and deck numbers.
local presets = {
    ["vhs-new"] = {
        wear = 0.06, dropout = 0.001, grain = 0.020, scratch = 0.0, crease = 0.0,
        doc = 1.0, sparkle = 0.8, chroma_phase = 0.025, wobble = 0.006,
        band = 48.0, life = 96.0, position = 0.0,
        speed = "sp", decode = "2d",
        headswitch = 3.0, headswitch_offset = 1.0, headswitch_shift = 0.04,
        wow = 0.0002, flutter = 0.0001,
        framelock = 0.995, tbc = 0.96, noise = 0.65, tracking = 0.0,
        tracking_height = 12.0, tracking_scroll = 0.0,
        chroma_blend = 0.0, chroma_loss = 0.0, chroma_delay = 0.0, chroma_delay_v = 0.0,
        frame_bounce = 0.004, frame_step = 2.0, breath = 0.025,
        flag = 0.0, generations = 1,
    },
    ["vhs-used"] = {
        wear = 0.35, dropout = 0.005, grain = 0.035, scratch = 0.1, crease = 0.05,
        doc = 1.0, sparkle = 0.8, chroma_phase = 0.04, wobble = 0.010,
        band = 40.0, life = 80.0, position = 0.0,
        speed = "sp", decode = "2d",
        headswitch = 4.0, headswitch_offset = 1.0, headswitch_shift = 0.05,
        wow = 0.0004, flutter = 0.0002,
        framelock = 0.98, tbc = 0.90, noise = 0.8, tracking = 0.02,
        tracking_height = 16.0, tracking_scroll = 0.0,
        chroma_blend = 0.1, chroma_loss = 0.000025, chroma_delay = 0.0, chroma_delay_v = 0.0,
        frame_bounce = 0.006, frame_step = 2.0, breath = 0.04,
        flag = 0.0, generations = 1,
    },
    ["vhs-old"] = {
        wear = 0.70, dropout = 0.012, grain = 0.050, scratch = 0.3, crease = 0.2,
        doc = 0.9, sparkle = 0.85, chroma_phase = 0.06, wobble = 0.015,
        band = 32.0, life = 64.0, position = 0.0,
        speed = "lp", decode = "comb2",
        headswitch = 6.0, headswitch_offset = 1.0, headswitch_shift = 0.08,
        wow = 0.0008, flutter = 0.0004,
        framelock = 0.95, tbc = 0.80, noise = 1.0, tracking = 0.10,
        tracking_height = 24.0, tracking_scroll = 0.2,
        chroma_blend = 0.2, chroma_loss = 0.0001, chroma_delay = 5.0, chroma_delay_v = 0.0,
        frame_bounce = 0.008, frame_step = 2.0, breath = 0.06,
        flag = 0.05, generations = 1,
    },
    ["vhs-veryold"] = {
        wear = 1.20, dropout = 0.025, grain = 0.070, scratch = 0.6, crease = 0.5,
        doc = 0.8, sparkle = 0.9, chroma_phase = 0.10, wobble = 0.025,
        band = 24.0, life = 48.0, position = 0.0,
        speed = "ep", decode = "notch",
        headswitch = 9.0, headswitch_offset = 1.0, headswitch_shift = 0.12,
        wow = 0.0015, flutter = 0.0008,
        framelock = 0.85, tbc = 0.65, noise = 1.3, tracking = 0.3,
        tracking_height = 32.0, tracking_scroll = 0.3,
        chroma_blend = 0.3, chroma_loss = 0.0003, chroma_delay = 6.0, chroma_delay_v = 1.0,
        frame_bounce = 0.012, frame_step = 2.0, breath = 0.08,
        flag = 0.3, generations = 2,
    },
    ["vhs-dub"] = {
        wear = 0.15, dropout = 0.0025, grain = 0.030, scratch = 0.05, crease = 0.05,
        doc = 1.0, sparkle = 0.8, chroma_phase = 0.04, wobble = 0.008,
        band = 40.0, life = 80.0, position = 0.0,
        speed = "lp", decode = "2d",
        headswitch = 4.0, headswitch_offset = 1.0, headswitch_shift = 0.06,
        wow = 0.0004, flutter = 0.0002,
        framelock = 0.97, tbc = 0.90, noise = 0.8, tracking = 0.0,
        tracking_height = 16.0, tracking_scroll = 0.0,
        chroma_blend = 0.2, chroma_loss = 0.00005, chroma_delay = 5.0, chroma_delay_v = 0.0,
        frame_bounce = 0.006, frame_step = 2.0, breath = 0.04,
        flag = 0.0, generations = 3,
    },
    ["vhs-baddeck"] = {
        wear = 0.06, dropout = 0.001, grain = 0.020, scratch = 0.0, crease = 0.0,
        doc = 1.0, sparkle = 0.8, chroma_phase = 0.025, wobble = 0.006,
        band = 48.0, life = 96.0, position = 0.0,
        speed = "sp", decode = "notch",
        headswitch = 10.0, headswitch_offset = 2.0, headswitch_shift = 0.15,
        wow = 0.0015, flutter = 0.0008,
        framelock = 0.50, tbc = 0.20, noise = 1.0, tracking = 0.6,
        tracking_height = 48.0, tracking_scroll = 0.5,
        chroma_blend = 0.1, chroma_loss = 0.0001, chroma_delay = 0.0, chroma_delay_v = 0.0,
        frame_bounce = 0.02, frame_step = 2.0, breath = 0.04,
        flag = 0.7, generations = 1,
    },
    ["vhs-damaged"] = {
        wear = 1.0, dropout = 0.03, grain = 0.060, scratch = 0.8, crease = 0.8,
        doc = 0.7, sparkle = 0.9, chroma_phase = 0.08, wobble = 0.02,
        band = 30.0, life = 70.0, position = 0.0,
        speed = "lp", decode = "comb1",
        headswitch = 10.0, headswitch_offset = 1.0, headswitch_shift = 0.12,
        wow = 0.001, flutter = 0.0005,
        framelock = 0.70, tbc = 0.60, noise = 1.2, tracking = 0.5,
        tracking_height = 40.0, tracking_scroll = 0.4,
        chroma_blend = 0.2, chroma_loss = 0.0003, chroma_delay = 5.0, chroma_delay_v = 1.0,
        frame_bounce = 0.015, frame_step = 2.0, breath = 0.06,
        flag = 0.5, generations = 1,
        rf_loss = 0.0, killer = 0.0, sync_loss = 0.0,
    },
    -- Not wear: a tape that physically takes the signal away. The patch of
    -- bad tape is where the killer trips, the servo lets go and the raster
    -- becomes snow; everything outside it still looks like a worn tape.
    ["vhs-wrecked"] = {
        wear = 1.2, dropout = 0.04, grain = 0.080, scratch = 1.0, crease = 1.0,
        doc = 0.5, sparkle = 1.0, chroma_phase = 0.20, wobble = 0.06,
        band = 20.0, life = 40.0, position = 0.0,
        speed = "ep", decode = "notch",
        headswitch = 12.0, headswitch_offset = 1.0, headswitch_shift = 0.20,
        wow = 0.002, flutter = 0.001,
        framelock = 0.20, tbc = 0.10, noise = 1.6, tracking = 0.9,
        tracking_height = 64.0, tracking_scroll = 0.8,
        chroma_blend = 0.4, chroma_loss = 0.002, chroma_delay = 8.0, chroma_delay_v = 2.0,
        frame_bounce = 0.05, frame_step = 2.0, breath = 0.12,
        flag = 1.0, generations = 2,
        rf_loss = 1.0, killer = 1.0, sync_loss = 1.0, roll_speed = 1.0 / 90.0, roll_skew = 0.06, dead_density = 0.05,
    },
}

local preset_order = { "vhs-new", "vhs-used", "vhs-old", "vhs-veryold", "vhs-dub", "vhs-baddeck", "vhs-damaged", "vhs-wrecked" }
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
local tape_dropout = option("dropout", preset_value("dropout", 0.001 + 0.0005 * (tape_seed % 5) / 4.0), 0.0, 1.0)
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
local tape_length = math.floor(option("tape-length", preset_value("tape_length", 3600.0), 240.0, 65536.0))

local tape_deck = option_string("speed", preset_value("speed", "sp"), tape_speeds)
local tape_speed = tape_speeds[tape_deck]

-- Decode mode numbers must match the #if branches in NTSC_fullDecodeYIQ.
local decode_names = { notch = 0, comb1 = 1, comb2 = 2, ["2d"] = 3, ["2d-adaptive"] = 4 }
local decode_name = option_string("decode", preset_value("decode", "2d"), decode_names)
local decode_mode = decode_names[decode_name]

-- The comb and 2D luma estimators subtract some chroma from the composite.
-- Put it back so a flat colour keeps the same saturation as the notch decoder.
local comb_saturation_boost = 1.0
if decode_name == "comb1" or decode_name == "comb2" then
    comb_saturation_boost = 1.4
elseif decode_name == "2d" then
    comb_saturation_boost = 2.2
elseif decode_name == "2d-adaptive" then
    comb_saturation_boost = 1.5
end

-- Deck condition. Head switch, wow/flutter, frame lock and TBC are the deck's,
-- not the tape's; only the head-to-tape contact is shared. Head-switch values
-- are in lines, not in frame fractions.
local deck_headswitch_height = option("headswitch", preset_value("headswitch", 4.0), 0.0, 24.0)
local deck_headswitch_offset = option("headswitch-offset", preset_value("headswitch_offset", 1.0), 0.0, 24.0)
local deck_headswitch_shift = option("headswitch-shift", preset_value("headswitch_shift", 0.05), -0.5, 0.5)
local deck_headswitch_power = option("headswitch-power", 1.5, 1.0, 4.0)
local deck_headswitch_jitter = option("headswitch-jitter", 0.002, 0.0, 0.05)
local deck_headswitch_midline = option("headswitch-midline", 1.0, 0.0, 1.0)
local deck_headswitch_midline_pos = option("headswitch-midline-pos", 0.8, 0.0, 1.0)
local deck_headswitch_midline_jitter = option("headswitch-midline-jitter", 0.03, 0.0, 0.2)
local deck_wow = option("wow", preset_value("wow", 0.0001 + 0.0002 * deck_rand(2)), 0.0, 0.01)
local deck_flutter = option("flutter", preset_value("flutter", 0.00005 + 0.00008 * deck_rand(3)), 0.0, 0.005)
local deck_framelock = option("framelock", preset_value("framelock", 0.985 + 0.013 * deck_rand(4)), 0.0, 1.0)
local deck_tbc = option("tbc", preset_value("tbc", 0.94 + 0.05 * deck_rand(5)), 0.0, 1.0)
local deck_noise = option("noise", preset_value("noise", 0.55 + 0.25 * deck_rand(6)), 0.0, 2.0)
local deck_tracking = option("tracking", preset_value("tracking", 0.0), 0.0, 1.0)
local deck_tracking_height = option("tracking-height", preset_value("tracking_height", 24.0), 0.0, 120.0)
local deck_tracking_scroll = option("tracking-scroll", preset_value("tracking_scroll", 0.0), 0.0, 1.0)
local deck_tracking_wave = option("tracking-wave", 15.0, 0.0, 50.0)
local deck_tracking_snow = option("tracking-snow", 0.10, 0.0, 1.0)
local deck_tracking_noise = option("tracking-noise", 0.25, 0.0, 1.0)
local deck_tracking_anisotropy = option("tracking-anisotropy", 0.25, 0.0, 1.0)
local deck_frame_bounce = option("frame-bounce", preset_value("frame_bounce", 0.004), 0.0, 0.1)
local deck_frame_step = option("frame-step", preset_value("frame_step", 2.0), 1.0, 12.0)
local deck_track_phase = deck_rand(13)
local deck_wow_phase = deck_rand(14)
local deck_flutter_phase = deck_rand(15)

-- Chroma path.
local chroma_delay_h = option("chroma-delay-h", preset_value("chroma_delay", 0.0), -32.0, 32.0)
local chroma_delay_v = option("chroma-delay-v", preset_value("chroma_delay_v", 0.0), -8.0, 8.0)
local chroma_blend = option("chroma-blend", preset_value("chroma_blend", 0.0), 0.0, 1.0)
local chroma_loss = option("chroma-loss", preset_value("chroma_loss", 0.0), 0.0, 0.1)

-- FBM grain.
local grain_frequency = option("grain-frequency", 0.25, 0.01, 1.0)
local grain_detail = math.floor(option("grain-detail", 2, 1, 5))

-- How many record/playback passes the picture makes. 1 is one deck, 2 is a
-- single dub. Every pass applies the VHS record bandwidth again, so softness,
-- noise and chroma phase error accumulate like they do on real copies. A new
-- tape on a working deck is a single pass.
local generations = math.floor(option("generations", preset_value("generations", 1), 1, 4))

local tape_slant <const> = 2.0 / base_tall
local tape_breath = option("breath", preset_value("breath", 0.02 + 0.01 * tape_rand(13)), 0.0, 0.1)
local flag_amount = option("flag", preset_value("flag", 0.0), 0.0, 1.0)

-- Signal loss. A bad patch of tape does not just dirty the picture: it takes
-- the colour and the sync away. RF quality is a scalar per frame (one track
-- under the head), derived from the track index, so the same spot on the tape
-- breaks on every pass and -tape/-seed stay reproducible.
local rf_loss = option("rf-loss", preset_value("rf_loss", 0.0), 0.0, 1.0)
local killer_amount = option("killer", preset_value("killer", 0.0), 0.0, 1.0)
local sync_loss = option("sync-loss", preset_value("sync_loss", 0.0), 0.0, 1.0)
local roll_speed = option("roll-speed", preset_value("roll_speed", 1.0 / 90.0), 1.0 / 600.0, 1.0 / 10.0)
local roll_skew = option("roll-skew", preset_value("roll_skew", 0.06), 0.0, 0.5)

-- How dense the raster is while there is no RF at all. This is the V9
-- analogue of V8's VHS_DROPOUT_STRENGTH: low is sparse dashes on black,
-- high is a nearly white raster with coloured dashes.
local dead_density = option("dead-density", preset_value("dead_density", 0.05), 0.0, 1.0)

-- Peak level of baseband snow (dropout head, scratch, tracking, crease).
-- A hole is lost signal, but a bar at full white reads as a blown highlight
-- rather than noise, so the level stays below the clip point.
local snow_level = option("snow-level", preset_value("snow_level", 0.55), 0.0, 1.0)

-- Speed-dependent bandwidth. A VHS line is 227.5 subcarrier cycles long and
-- the line rate is 15734 Hz, so a cutoff in Hz maps to a fixed number of
-- cycles per line. The pre-band Gaussian gets its sigma from the same number.
local line_rate <const> = 15734.0
local luma_cols = math.floor(2.0 * tape_speed.luma / line_rate + 0.5)
local chroma_cols = math.floor(2.0 * tape_speed.chroma / line_rate + 0.5)
luma_cols = math.min(math.max(luma_cols, 64), 2048)
chroma_cols = math.min(math.max(chroma_cols, 8), 512)

local downsample_wide <const> = luma_cols + 3
local pixel_rate <const> = base_wide * line_rate
local pre_y_sigma = 0.1874 * pixel_rate / tape_speed.luma
local pre_iq_sigma = 0.1874 * pixel_rate / tape_speed.chroma
local pre_y_radius = math.min(12, math.ceil(3.0 * pre_y_sigma))
local pre_iq_radius = math.min(24, math.ceil(3.0 * pre_iq_sigma / 2.0))

-- Periods for the periodic tape noise. Every tape-coordinate sample has to
-- wrap at the same track count, otherwise the loop seam is visible. Periods
-- are fractional; Tape_noisePeriodic handles the partial last cell.
local function tape_loop_period(divisor)
    return math.max(1.0, tape_length / math.max(divisor, 1e-6))
end

local tape_wear_period_1 = tape_loop_period(tape_life)
local tape_wear_period_2 = tape_loop_period(tape_life * 0.25)
local tape_wear_period_3 = tape_loop_period(tape_life * 0.0625)
local tape_scratch_period = tape_loop_period(math.max(4.0, tape_life * 0.25))
local tape_scrape_period = tape_loop_period(6.0)

-- A bad patch on the tape. It has to wrap at the same track count as the
-- other tape noise, or the loop seam becomes a permanent failure.
local tape_rf_patch_tracks = math.max(12.0, tape_life * 0.35)
local tape_rf_period = tape_loop_period(tape_rf_patch_tracks)

-- VHS composite timing. One active line is 227.5 colorburst cycles, and the
-- encode runs at 2 * base_wide, so the wavelength has to follow the target
-- width instead of being a magic 3.0 tuned for 1280.
local encode_wide <const> = 2.0 * base_wide
local colorburst_period <const> = encode_wide / 227.5
local colorburst_wavelength <const> = colorburst_period / 2.0
local am_wavelength <const> = math.max(2.0, colorburst_wavelength / 2.0)

-- About 3 MHz of luma: 3e6 / 15734 = 190.7 cycles per line. This is the NTSC
-- encoder/decoder limit; the tape-speed record limit lives in pre_band.
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
    "VHS hop: preset=%s tape=%d deck=%d gen=%d speed=%s decode=%s wear=%.3f dropout=%.4f grain=%.4f band=%.2f life=%.2f pos=%.1f len=%d tracking=%.3f track_h=%.1f track_scroll=%.2f hs=%.1f/%.1f/%.3f wow=%.5f flutter=%.5f framelock=%.3f tbc=%.3f noise=%.3f crease=%.3f flag=%.3f chroma=%.1f/%.1f/%.2f/%.4f grain_freq=%.2f rf=%.2f killer=%.2f sync=%.2f roll=%.5f skew=%.3f dead=%.2f",
    preset_name or "new(default)", tape_seed, deck_seed, generations, tape_deck, decode_name, tape_wear, tape_dropout, tape_grain, tape_band, tape_life,
    tape_position, tape_length, deck_tracking, deck_tracking_height, deck_tracking_scroll, deck_headswitch_height, deck_headswitch_offset,
    deck_headswitch_shift, deck_wow, deck_flutter, deck_framelock, deck_tbc, deck_noise, tape_crease, flag_amount, chroma_delay_h,
    chroma_delay_v, chroma_blend, chroma_loss, grain_frequency, rf_loss, killer_amount, sync_loss, roll_speed, roll_skew, dead_density))

-- Define helpers keep the big shader string free of a 40-argument format call.
local function define(name, value)
    return string.format("#define %s (%s)\n", name, value)
end

local defines = table.concat({
    "#ifndef GEN_ENTROPY\n#define GEN_ENTROPY (0U)\n#endif\n",
    define("DECODE_MODE", string.format("%d", decode_mode)),
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
    -- Signal loss. The thresholds are the deck's, not the tape's: a killer
    -- snaps at its burst threshold and a servo needs a little more RF than
    -- the decoder does. Both are narrow on purpose, so the picture breaks
    -- instead of fading.
    define("TAPE_RF_LOSS", string.format("%.6f", rf_loss)),
    define("TAPE_RF_PATCH_TRACKS", string.format("%.4f", tape_rf_patch_tracks)),
    define("TAPE_RF_PERIOD", string.format("%.1f", tape_rf_period)),
    define("TAPE_LOCK_LO", "0.35"),
    define("TAPE_LOCK_HI", "0.55"),
    define("TAPE_KILLER_LO", "0.45"),
    define("TAPE_KILLER_HI", "0.52"),
    define("TAPE_DEAD_LO", "0.05"),
    define("TAPE_DEAD_HI", "0.15"),
    define("TAPE_DEAD_DENSITY", string.format("%.6f", dead_density)),
    define("VHS_SNOW_LEVEL", string.format("%.6f", snow_level)),
    define("TAPE_KILLER", string.format("%.6f", killer_amount)),
    define("TAPE_SYNC_LOSS", string.format("%.6f", sync_loss)),
    define("TAPE_ROLL_SPEED", string.format("%.8f", roll_speed)),
    define("TAPE_ROLL_SKEW", string.format("%.6f", roll_skew)),
    define("TAPE_ROLL_STEP", "2.0"),
    define("TAPE_SLANT", string.format("%.6f", tape_slant)),
    define("TAPE_ROWS", "480.0"),
    define("TAPE_ROWS_PER_FIELD", "240.0"),
    define("TAPE_TRACKS_PER_FRAME", "2.0"),
    define("TAPE_LENGTH_TRACKS", string.format("%.4f", tape_length)),
    define("TAPE_BAND_ROWS", string.format("%.4f", tape_band)),
    define("TAPE_LIFE_TRACKS", string.format("%.4f", tape_life)),
    define("TAPE_WEAR_PERIOD_1", string.format("%.1f", tape_wear_period_1)),
    define("TAPE_WEAR_PERIOD_2", string.format("%.1f", tape_wear_period_2)),
    define("TAPE_WEAR_PERIOD_3", string.format("%.1f", tape_wear_period_3)),
    define("TAPE_SCRATCH_PERIOD", string.format("%.1f", tape_scratch_period)),
    define("TAPE_SCRAPE_PERIOD", string.format("%.1f", tape_scrape_period)),
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
    define("DECK_HEADSWITCH_HEIGHT", string.format("%.4f", deck_headswitch_height)),
    define("DECK_HEADSWITCH_OFFSET", string.format("%.4f", deck_headswitch_offset)),
    define("DECK_HEADSWITCH_SHIFT", string.format("%.6f", deck_headswitch_shift)),
    define("DECK_HEADSWITCH_POWER", string.format("%.4f", deck_headswitch_power)),
    define("DECK_HEADSWITCH_JITTER", string.format("%.6f", deck_headswitch_jitter)),
    define("DECK_HEADSWITCH_MIDLINE", string.format("%.1f", deck_headswitch_midline)),
    define("DECK_HEADSWITCH_MIDLINE_POS", string.format("%.5f", deck_headswitch_midline_pos)),
    define("DECK_HEADSWITCH_MIDLINE_JITTER", string.format("%.5f", deck_headswitch_midline_jitter)),
    define("DECK_WOW", string.format("%.6f", deck_wow)),
    define("DECK_FLUTTER", string.format("%.6f", deck_flutter)),
    define("DECK_FRAMELOCK", string.format("%.6f", deck_framelock)),
    define("DECK_FRAME_BOUNCE", string.format("%.6f", deck_frame_bounce)),
    define("DECK_FRAME_STEP", string.format("%.3f", deck_frame_step)),
    define("DECK_TBC", string.format("%.6f", deck_tbc)),
    define("DECK_NOISE", string.format("%.6f", deck_noise)),
    define("DECK_TRACKING", string.format("%.6f", deck_tracking)),
    define("DECK_TRACKING_HEIGHT", string.format("%.3f", deck_tracking_height)),
    define("DECK_TRACKING_SCROLL", string.format("%.5f", deck_tracking_scroll)),
    define("DECK_TRACKING_WAVE", string.format("%.5f", deck_tracking_wave)),
    define("DECK_TRACKING_SNOW", string.format("%.6f", deck_tracking_snow)),
    define("DECK_TRACKING_NOISE", string.format("%.5f", deck_tracking_noise)),
    define("DECK_TRACKING_ANISOTROPY", string.format("%.5f", deck_tracking_anisotropy)),
    define("DECK_TRACK_PHASE", string.format("%.6f", deck_track_phase)),
    define("DECK_WOW_PHASE", string.format("%.6f", deck_wow_phase)),
    define("DECK_FLUTTER_PHASE", string.format("%.6f", deck_flutter_phase)),
    define("CHROMA_DELAY_H", string.format("%.4f", chroma_delay_h)),
    define("CHROMA_DELAY_V", string.format("%.4f", chroma_delay_v)),
    define("CHROMA_BLEND", string.format("%.6f", chroma_blend)),
    define("CHROMA_LOSS", string.format("%.6f", chroma_loss)),
    define("GRAIN_FREQUENCY", string.format("%.5f", grain_frequency)),
    define("GRAIN_DETAIL", string.format("%d", grain_detail)),
    define("DECODE_WINDOW_WIDTH", "16"),
    define("COMB_DECODE_WINDOW_WIDTH", "5"),
    define("COMB_SATURATION_BOOST", string.format("%.3f", comb_saturation_boost)),
    define("ENCODE_WINDOW_WIDTH", "8"),
    define("PRE_BAND_Y_SIGMA", string.format("%.6f", pre_y_sigma)),
    define("PRE_BAND_IQ_SIGMA", string.format("%.6f", pre_iq_sigma)),
    define("PRE_BAND_Y_RADIUS", string.format("%d", pre_y_radius)),
    define("PRE_BAND_IQ_RADIUS", string.format("%d", pre_iq_radius)),
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
    define("VHS_DROPOUT_MIN_STEPS", "0"),
    define("VHS_MAXRES_Y", string.format("vec2(%.1f, 480.0)", luma_cols)),
    define("VHS_MAXRES_IQ", string.format("vec2(%.1f, 480.0)", chroma_cols)),
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

    float Common_field(vec2 uv)
    {
        return mod(floor(uv.y * TAPE_ROWS), 2.0);
    }

    uint Common_newRngStateEx(vec2 uv, uint entropy)
    {
        return uint(uint(uv.x) * 1973U + uint(uv.y) * 9277U + uint(iFrame) * 26699U
            + entropy * 34961U + GEN_ENTROPY * 104729U + DECK_SEED * 41077U
            + uint(Common_field(uv)) * 15485863U) | 1U;
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

    float Common_corner(vec2 cell, uint salt, uint seed)
    {
        uint state = Common_hashState(cell, salt, seed);
        return Common_randomFloat(state);
    }

    // 2D value noise with an explicit seed. Tape defects use TAPE_SEED, deck
    // instabilities use DECK_SEED; keeping the two apart is what makes -tape
    // and -deck independent.
    float Common_noise(vec2 p, uint salt, uint seed)
    {
        vec2 i = floor(p);
        vec2 f = fract(p);
        f = f * f * (3.0 - 2.0 * f);

        float c00 = Common_corner(i + vec2(0.0, 0.0), salt, seed);
        float c10 = Common_corner(i + vec2(1.0, 0.0), salt, seed);
        float c01 = Common_corner(i + vec2(0.0, 1.0), salt, seed);
        float c11 = Common_corner(i + vec2(1.0, 1.0), salt, seed);

        return mix(mix(c00, c10, f.x), mix(c01, c11, f.x), f.y);
    }

    float Deck_noise(vec2 p, uint salt)
    {
        return Common_noise(p, salt, DECK_SEED);
    }

    // Periodic-in-track variant. periodY is a number of noise cells (it may
    // be fractional); the track coordinate wraps at the same period, so the
    // tape loop has no visible seam.
    float Tape_noisePeriodic(vec2 p, uint salt, float periodY)
    {
        vec2 i = floor(p);
        vec2 f = fract(p);
        f.x = f.x * f.x * (3.0 - 2.0 * f.x);

        float period = max(periodY, 1.0);
        float y0 = mod(i.y, period);
        float y1 = mod(i.y + 1.0, period);

        // The last cell before the wrap is usually partial. Interpolate over
        // its real width, otherwise the loop seam lands halfway between two
        // noise cells and the whole tape pattern jumps.
        float cellEnd = min(i.y + 1.0, period);
        float fy = (p.y - i.y) / max(cellEnd - i.y, 1e-6);
        fy = fy * fy * (3.0 - 2.0 * fy);

        float c00 = Common_corner(vec2(i.x, y0), salt, TAPE_SEED);
        float c10 = Common_corner(vec2(i.x + 1.0, y0), salt, TAPE_SEED);
        float c01 = Common_corner(vec2(i.x, y1), salt, TAPE_SEED);
        float c11 = Common_corner(vec2(i.x + 1.0, y1), salt, TAPE_SEED);

        return mix(mix(c00, c10, f.x), mix(c01, c11, f.x), fy);
    }

    // Screen-space value noise and FBM, used for the grain. This is not tape
    // noise: it changes every frame and is not tied to the tape coordinates.
    float Common_valueNoise(vec2 p, uint seed)
    {
        vec2 i = floor(p);
        vec2 f = fract(p);
        f = f * f * (3.0 - 2.0 * f);

        float c00 = Common_corner(i + vec2(0.0, 0.0), 0U, seed);
        float c10 = Common_corner(i + vec2(1.0, 0.0), 0U, seed);
        float c01 = Common_corner(i + vec2(0.0, 1.0), 0U, seed);
        float c11 = Common_corner(i + vec2(1.0, 1.0), 0U, seed);

        return mix(mix(c00, c10, f.x), mix(c01, c11, f.x), f.y);
    }

    float Common_fbm(vec2 p, uint seed, int detail, float gain)
    {
        float sum = 0.0;
        float amp = 1.0;
        float norm = 0.0;

        for (int octave = 0; octave < 5; ++octave) {
            if (octave >= detail)
                break;

            sum += amp * Common_valueNoise(p, seed + uint(octave) * 1013U);
            norm += amp;
            p *= 2.0;
            amp *= gain;
        }

        return sum / max(norm, 1e-4);
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
        return mod(iFrame * TAPE_TRACKS_PER_FRAME + Tape_field(uv) + TAPE_POSITION, TAPE_LENGTH_TRACKS);
    }

    // 0..1 along one field track. A constant value is a horizontal band.
    float Tape_along(vec2 uv)
    {
        return (floor(uv.y * TAPE_ROWS_PER_FIELD) + 0.5) / TAPE_ROWS_PER_FIELD;
    }

    // The subcarrier phase advances by half a cycle per field line. Frame rows
    // alternate fields, so the phase must be based on the field line, not on
    // uv.y * TAPE_ROWS; otherwise a comb filter would average same-phase chroma.
    float Tape_linePhase(vec2 uv)
    {
        float fieldLine = floor(uv.y * TAPE_ROWS_PER_FIELD);
        return mod(fieldLine + Common_field(uv), 2.0) * PI;
    }

    // Wear as a function of the tape, not of the frame. Each octave has its
    // own height in field lines (TAPE_BAND_ROWS) and its own length in tracks
    // (TAPE_LIFE_TRACKS), so height and lifetime are not locked together.
    float Tape_wear(vec2 uv)
    {
        float along = Tape_along(uv) + TAPE_SLANT * uv.x;
        float track = Tape_track(uv);

        float n = 0.55 * Tape_noisePeriodic(vec2(along * (TAPE_ROWS_PER_FIELD / TAPE_BAND_ROWS), track / TAPE_LIFE_TRACKS), 20U, TAPE_WEAR_PERIOD_1)
                + 0.30 * Tape_noisePeriodic(vec2(along * (TAPE_ROWS_PER_FIELD / (TAPE_BAND_ROWS * 0.25)), track / (TAPE_LIFE_TRACKS * 0.25)), 21U, TAPE_WEAR_PERIOD_2)
                + 0.15 * Tape_noisePeriodic(vec2(along * (TAPE_ROWS_PER_FIELD / (TAPE_BAND_ROWS * 0.0625)), track / (TAPE_LIFE_TRACKS * 0.0625)), 22U, TAPE_WEAR_PERIOD_3);

        float floorWear = TAPE_FLOOR * step(0.005, TAPE_WEAR);
        return clamp(TAPE_WEAR * smoothstep(0.52, 0.76, n) + floorWear, 0.0, 1.0);
    }

    // ---- Signal loss ------------------------------------------------------
    // The head reads one track at a time, so "is there usable RF right now"
    // is a scalar per frame, not a per-pixel property. Deriving it from the
    // track and not from a per-frame random is what makes a bad cassette
    // break at the same spot every time it is played.

    float Tape_trackNow()
    {
        return mod(iFrame * TAPE_TRACKS_PER_FRAME + TAPE_POSITION, TAPE_LENGTH_TRACKS);
    }

    // 0..1 usable RF. Oxide wear alone never kills the picture; a bad patch
    // of tape does. This is what the killer and the servo react to.
    float Tape_rfNow()
    {
        float wear = Tape_wear(vec2(0.5, 0.5));
        float patch = Tape_noisePeriodic(vec2(2.5, Tape_trackNow() / TAPE_RF_PATCH_TRACKS), 60U, TAPE_RF_PERIOD);
        return clamp(1.0 - 0.35 * wear - TAPE_RF_LOSS * smoothstep(0.55, 0.85, patch), 0.0, 1.0);
    }

    // 1 while the servo has no lock. A deck with a good frame lock hides most
    // of this, which is why the same tape rolls on one deck and not another.
    float Tape_unlocked()
    {
        float raw = 1.0 - smoothstep(TAPE_LOCK_LO, TAPE_LOCK_HI, Tape_rfNow());
        return clamp(raw * (1.0 - 0.65 * DECK_FRAMELOCK), 0.0, 1.0) * TAPE_SYNC_LOSS;
    }

    // The killer snaps, it does not fade: below the burst threshold the chroma
    // channel is muted, not desaturated. 1 is alive, 0 is black and white.
    float Tape_colorKiller()
    {
        return mix(1.0, smoothstep(TAPE_KILLER_LO, TAPE_KILLER_HI, Tape_rfNow()), TAPE_KILLER);
    }

    // 1 when there is no RF at all: the raster is snow, not a dark picture.
    float Tape_deadRf()
    {
        return 1.0 - smoothstep(TAPE_DEAD_LO, TAPE_DEAD_HI, Tape_rfNow());
    }

    // Vertical hold lost: the frame advances by a fixed fraction per frame, so
    // it rolls at a steady rate instead of jittering. Snapped to whole lines,
    // because the field phase steps rather than glides.
    float Tape_roll()
    {
        float phase = fract(float(iFrame) * TAPE_ROLL_SPEED) * Tape_unlocked();
        return floor(phase * TAPE_ROWS / TAPE_ROLL_STEP + 0.5) * TAPE_ROLL_STEP / TAPE_ROWS;
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

        float wow = sin(t * 6.2831853 * 0.73 + DECK_WOW_PHASE * 6.2831853) * DECK_WOW;
        float flutter = sin(t * 6.2831853 * 6.1 + DECK_FLUTTER_PHASE * 6.2831853 + along * 47.0) * DECK_FLUTTER;
        float scrape = (2.0 * Tape_noisePeriodic(vec2(along * 120.0, track / 6.0), 30U, TAPE_SCRAPE_PERIOD) - 1.0) * TAPE_WOBBLE * wear;
        float crease = Tape_crease(uv) * TAPE_CREASE_SHIFT;

        return (wow + flutter + scrape + crease) * (1.0 - DECK_TBC);
    }

    // Whole-frame vertical sync offset, in frame fractions. Only a deck that
    // is not frame-locked can slip; the offset snaps in whole lines because
    // the field phase does.
    float Tape_frameOffset()
    {
        float slow = 2.0 * Deck_noise(vec2(float(iFrame) / 72.0, 0.5), 70U) - 1.0;
        float fast = 2.0 * Deck_noise(vec2(float(iFrame) / 12.0, 0.5), 71U) - 1.0;
        float amount = (1.0 - DECK_FRAMELOCK) * DECK_FRAME_BOUNCE;
        float bounce = (0.6 * slow + 0.4 * fast) * amount;

        // A frame-locked deck only bounces; an unlocked one also rolls. The
        // rows that wrap are marked as dead reads by the dropout passes, so
        // the seam shows up as a band of noise, like the vertical blanking
        // interval on a real rolling picture.
        float snapped = floor(bounce * TAPE_ROWS / DECK_FRAME_STEP + 0.5) * DECK_FRAME_STEP / TAPE_ROWS;
        return snapped + Tape_roll();
    }

    // Head-to-tape contact breathes over seconds: the whole frame slowly
    // gains and loses level. One slow noise, so it is global and smooth.
    float Tape_breath()
    {
        float n = Deck_noise(vec2(float(iFrame) / 72.0, 0.5), 60U);
        return 2.0 * n - 1.0;
    }

    // A scratch is a longitudinal line. It is permanent in tape coordinates;
    // only its depth varies with the track that is being read. The row is a
    // field line, so it spans two adjacent frame rows (one per field).
    float Tape_scratch(vec2 uv)
    {
        float row = floor(uv.y * TAPE_ROWS_PER_FIELD);
        float track = Tape_track(uv);

        float pos = Common_corner(vec2(0.0, row), 50U, TAPE_SEED);
        float rate = 0.02 * TAPE_SCRATCH * clamp(TAPE_WEAR, 0.0, 1.0);
        float canScratch = step(1.0 - rate, pos);

        float severity = Tape_noisePeriodic(vec2(row * 0.7, track / max(4.0, TAPE_LIFE_TRACKS * 0.25)), 51U, TAPE_SCRATCH_PERIOD);
        return canScratch * smoothstep(0.35, 0.65, severity);
    }

    // A mistracked deck reads the guard band instead of the track: a wide
    // horizontal band of noise and lost colour that scrolls through the
    // picture. 0 is perfect tracking, 1 is a completely lost servo.
    float Tape_scrollTracking(vec2 uv)
    {
        float amount = clamp(DECK_TRACKING, 0.0, 1.0) * DECK_TRACKING_SCROLL;
        if(amount <= 0.0)
            return 0.0;

        float y = fract(DECK_TRACK_PHASE + float(iFrame) * TAPE_TRACKING_SCROLL);
        float d = abs(uv.y - y);
        d = min(d, 1.0 - d);
        return smoothstep(TAPE_TRACKING_WIDTH, 0.0, d) * amount;
    }

    // A head that is not tracking well loses the bottom of the picture first.
    // This band is anchored to the bottom and fades upwards over
    // DECK_TRACKING_HEIGHT lines.
    float Tape_headTracking(vec2 uv)
    {
        if(DECK_TRACKING_HEIGHT <= 0.0)
            return 0.0;

        float height = max(DECK_TRACKING_HEIGHT / TAPE_ROWS, 1e-4);
        float t = clamp(uv.y / height, 0.0, 1.0);
        return pow(1.0 - t, 2.0) * clamp(DECK_TRACKING, 0.0, 1.0);
    }

    float Tape_tracking(vec2 uv)
    {
        return max(Tape_scrollTracking(uv), Tape_headTracking(uv));
    }

    // Transient snow. Instead of turning each pixel into an independent
    // random value, place short runs along the line and evaluate the wave
    // envelope at the current pixel. This is the GPU version of the
    // geometric-distribution snow generator in ntsc-rs.
    float Snow_transient(vec2 uv, uint salt, float intensity, float anisotropy)
    {
        float px = uv.x * iResolution.x;
        float row = floor(uv.y * TAPE_ROWS);
        uint rowState = Common_newRngStateEx(vec2(0.0, row), salt);
        float r = Common_randomFloat(rowState);
        float lineIntensity = mix(clamp(intensity, 0.0, 1.0), step(r, clamp(intensity, 0.0, 1.0)), clamp(anisotropy, 0.0, 1.0));

        if(lineIntensity <= 0.0)
            return 0.0;

        const float cellSize = 64.0;
        float cell = floor(px / cellSize);
        float result = 0.0;

        for(int k = -1; k <= 0; ++k) {
            float c = max(cell + float(k), 0.0);
            uint s = Common_newRngStateEx(vec2(c, row), salt + 1U);
            float start = (c + Common_randomFloat(s)) * cellSize;
            float len = mix(8.0, 64.0, Common_randomFloat(s));
            float amp = Common_randomFloat(s) * 2.0 - 1.0;
            float x = px - start;

            if(x >= 0.0 && x < len) {
                float freq = mix(len * 3.0, len * 5.0, Common_randomFloat(s));
                result += amp * pow(1.0 - x / len, 2.0) * cos(x * PI / freq);
            }
        }

        return result * lineIntensity;
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

        float phase = uv.x * PI / (COLORBURST_WAVELENGTH_ENCODER * pixelWidth) + Tape_linePhase(uv);
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

    // Luma estimate for a 1-line comb. The previous field line is two frame
    // rows away; its subcarrier phase is inverted by Tape_linePhase.
    float NTSC_lumaComb1(sampler2D sampler, vec2 uv, float pixelHeight)
    {
        float yPrev = clamp(uv.y + 2.0 * pixelHeight, 0.5 * pixelHeight, 1.0 - 0.5 * pixelHeight);
        return (texture(sampler, uv).x + texture(sampler, vec2(uv.x, yPrev)).x) * 0.5;
    }

    // Luma estimate for a 2-line comb.
    float NTSC_lumaComb2(sampler2D sampler, vec2 uv, float pixelHeight)
    {
        float yPrev = clamp(uv.y + 2.0 * pixelHeight, 0.5 * pixelHeight, 1.0 - 0.5 * pixelHeight);
        float yNext = clamp(uv.y - 2.0 * pixelHeight, 0.5 * pixelHeight, 1.0 - 0.5 * pixelHeight);
        return texture(sampler, uv).x * 0.5
            + texture(sampler, vec2(uv.x, yPrev)).x * 0.25
            + texture(sampler, vec2(uv.x, yNext)).x * 0.25;
    }

    // 5x3 luma estimate on the composite signal. Fixed weights are the digital
    // 2D filter from ntsc-rs; DECODE_MODE 4 turns them into gradient weights.
    float NTSC_luma2D(sampler2D sampler, vec2 uv, float xStep, float pixelHeight)
    {
        float yPrev = clamp(uv.y + 2.0 * pixelHeight, 0.5 * pixelHeight, 1.0 - 0.5 * pixelHeight);
        float yNext = clamp(uv.y - 2.0 * pixelHeight, 0.5 * pixelHeight, 1.0 - 0.5 * pixelHeight);

        float tl = texture(sampler, vec2(uv.x - xStep, yPrev)).x;
        float t = texture(sampler, vec2(uv.x, yPrev)).x;
        float tr = texture(sampler, vec2(uv.x + xStep, yPrev)).x;
        float l = texture(sampler, vec2(uv.x - xStep, uv.y)).x;
        float c = texture(sampler, uv).x;
        float r = texture(sampler, vec2(uv.x + xStep, uv.y)).x;
        float bl = texture(sampler, vec2(uv.x - xStep, yNext)).x;
        float b = texture(sampler, vec2(uv.x, yNext)).x;
        float br = texture(sampler, vec2(uv.x + xStep, yNext)).x;

        const float THRESHOLD = 1e-3;
        const float DIAG_LIMIT = 2.0;

#if DECODE_MODE == 4
        float g_x = (tr - tl) + (r - l) + (br - bl);
        float g_y = (bl - tl) + (b - t) + (br - tr);

        float gx_t = g_x / THRESHOLD;
        float gy_t = g_y / THRESHOLD;
        float gd1_t = (g_x + g_y) / (THRESHOLD * 1.41421356);
        float gd2_t = (g_x - g_y) / (THRESHOLD * 1.41421356);

        float a_x = 1.0 / (1.0 + gx_t * gx_t);
        float a_y = 1.0 / (1.0 + gy_t * gy_t);
        float norm = 0.25 / (a_x + a_y);

        float wx = a_x * norm;
        float wy = a_y * norm;
        float wd1 = norm / max(1.0 + gd1_t * gd1_t, DIAG_LIMIT / (THRESHOLD * THRESHOLD));
        float wd2 = norm / max(1.0 + gd2_t * gd2_t, DIAG_LIMIT / (THRESHOLD * THRESHOLD));
        float w_center = 0.5 + 2.0 * (wd1 + wd2);

        return c * w_center + (l + r) * wx + (t + b) * wy - (tl + br) * wd1 - (tr + bl) * wd2;
#else
        return c * 0.75 + (l + r + t + b) * 0.125 - (tl + tr + bl + br) * 0.0625;
#endif
    }

    vec3 NTSC_fullDecodeYIQ(sampler2D sampler, in vec2 uv, in vec2 pixelSize, uint rngRow, float tapeWear)
    {
        uint seed = rngRow;
        vec2 originalUV = uv;
        float pixelWidth = pixelSize.x * NTSC_SCALE;
        float pixelHeight = pixelSize.y;

        // Per-line chroma phase error. The encoder and decoder use the same
        // reference phase, so this is a real error rather than the old
        // per-line PI that cancelled itself out.
        float phaseNoise = Common_randomFloat(seed) * 2.0 - 1.0;
        phaseNoise *= TAPE_CHROMA_PHASE * (0.15 + 0.85 * clamp(tapeWear, 0.0, 1.0));
        phaseNoise += (Common_randomFloat(seed) * 2.0 - 1.0) * 0.004 * DECK_NOISE;

        float frequencyNoise = Common_randomFloat(seed) * 2.0 - 1.0;
        frequencyNoise *= TAPE_CHROMA_PHASE * 0.02 * clamp(tapeWear, 0.0, 1.0);

#if DECODE_MODE == 0
        vec3 yiq = vec3(0.0);
        float windowWeight = 0.0;

        for(int i = -DECODE_WINDOW_WIDTH; i <= DECODE_WINDOW_WIDTH; ++i) {
            float window = NTSC_cosineWindow(float(i) / float(DECODE_WINDOW_WIDTH + 1));
            vec2 uvWithOffset = vec2(uv.x + float(i) * pixelWidth, uv.y);
            vec2 originalUVWithOffset = vec2(originalUV.x + float(i) * pixelWidth, originalUV.y);
            float phase = originalUVWithOffset.x * PI / ((COLORBURST_WAVELENGTH_DECODER + frequencyNoise) * pixelWidth)
                + Tape_linePhase(originalUVWithOffset) + phaseNoise;

            float sincY = NTSC_sinc(float(i) / DECODE_LOWPASS_WAVELENGTH) / DECODE_LOWPASS_WAVELENGTH;
            float encodedSample = texture(sampler, uvWithOffset).x;

            yiq.x += encodedSample * sincY * window;
            yiq.y += encodedSample * sin(phase) * window;
            yiq.z += encodedSample * cos(phase) * window;
            windowWeight += window;
        }

        yiq.yz *= SATURATION / windowWeight;
        return yiq;
#else
        float xStep = 2.0 * pixelWidth;
        float lumaCenter;

#if DECODE_MODE == 1
        lumaCenter = NTSC_lumaComb1(sampler, uv, pixelHeight);
#elif DECODE_MODE == 2
        lumaCenter = NTSC_lumaComb2(sampler, uv, pixelHeight);
#else
        lumaCenter = NTSC_luma2D(sampler, uv, xStep, pixelHeight);
#endif

        vec2 iq = vec2(0.0);
        float weightSum = 0.0;

        for(int i = -COMB_DECODE_WINDOW_WIDTH; i <= COMB_DECODE_WINDOW_WIDTH; ++i) {
            float weight = NTSC_cosineWindow(float(i) / float(COMB_DECODE_WINDOW_WIDTH + 1));
            vec2 uvWithOffset = vec2(uv.x + float(i) * pixelWidth, uv.y);
            float encodedSample = texture(sampler, uvWithOffset).x;
            float lumaAt;

#if DECODE_MODE == 1
            lumaAt = NTSC_lumaComb1(sampler, uvWithOffset, pixelHeight);
#elif DECODE_MODE == 2
            lumaAt = NTSC_lumaComb2(sampler, uvWithOffset, pixelHeight);
#else
            lumaAt = NTSC_luma2D(sampler, uvWithOffset, xStep, pixelHeight);
#endif

            float phase = uvWithOffset.x * PI / (COLORBURST_WAVELENGTH_DECODER * pixelWidth)
                + Tape_linePhase(uvWithOffset) + phaseNoise;
            float chroma = encodedSample - lumaAt;

            iq.x += chroma * sin(phase) * weight;
            iq.y += chroma * cos(phase) * weight;
            weightSum += weight;
        }

        iq *= SATURATION * COMB_SATURATION_BOOST / weightSum;
        return vec3(lumaCenter, iq);
#endif
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
        // rows are marked as dead reads in the dropout pass. A lost vertical
        // hold also shears the picture sideways, so the roll goes diagonally
        // instead of straight up.
        float roll = Tape_roll();
        float bounce = Tape_frameOffset();
        vec4 color = texture(iChannel0, vec2(uv.x + shift + roll * TAPE_ROLL_SKEW, fract(uv.y + bounce)));
        fragColor.rgb = pow(color.rgb, vec3(1.0 / GAMMA_CORRECTION));
        fragColor.a = color.a;
    }
]]

-- The record-side bandwidth limit. This is where VHS loses detail: the tape
-- cannot carry the full source bandwidth. Sigma and loop radius come from the
-- selected tape speed (SP/LP/EP); the playback limit is the luma/chroma column
-- downsample later.
local pre_band_src <const> = common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord / iResolution.xy;
        vec2 px = 1.0 / iResolution.xy;

        float y = 0.0;
        float wySum = 0.0;
        for (int i = -PRE_BAND_Y_RADIUS; i <= PRE_BAND_Y_RADIUS; ++i) {
            float x = float(i);
            float wy = exp(-0.5 * (x / PRE_BAND_Y_SIGMA) * (x / PRE_BAND_Y_SIGMA));
            vec3 s = texture(iChannel0, uv + vec2(x * px.x, 0.0)).rgb * RGB_TO_YIQ;
            y += s.x * wy;
            wySum += wy;
        }
        y /= wySum;

        vec2 iq = vec2(0.0);
        float wiSum = 0.0;
        for (int i = -PRE_BAND_IQ_RADIUS; i <= PRE_BAND_IQ_RADIUS; ++i) {
            float x = float(i) * 2.0;
            float wi = exp(-0.5 * (x / PRE_BAND_IQ_SIGMA) * (x / PRE_BAND_IQ_SIGMA));
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
            float sineNoise = sin(uv.x * 200.0 + uv.y * -50.0 + fract(float(iFrame) * 0.37) * 3.1415927 * 2.0) * 0.065;
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

            fragColor.xyz = NTSC_fullDecodeYIQ(iChannel0, uv, pixelSize, rngStateRow, wear);
            fragColor.w = 1.0;
        }
    ]]
end

-- Repack the final YIQ into the two VHS playback bandwidths. The target width
-- is the number of samples needed for the selected tape speed's luma cutoff,
-- not a fixed fraction of the 2x composite width.
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
-- pixels along the raster, not over one pixel. The mask is a fresh set of
-- sparse impulses every frame; the blur below turns each impulse into a
-- bright head and a decaying tail, like V8's snow. `Tape_scratch` and the
-- tracking band raise the threshold at their lines instead of growing a
-- second generator.
local vhs_dropout = make_pass(common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord.xy / iResolution.xy;
        float wear = texture(iChannel0, uv).x;

        vec2 cell = vec2(floor(uv.x * 256.0), floor(uv.y * TAPE_ROWS_PER_FIELD));

        // With no RF at all nothing else matters: the generator runs at the
        // dead density and every other source of holes is suppressed below.
        // Otherwise their bands would show through the snow as bright stripes.
        float deadRf = Tape_deadRf();

        // Keep the non-uniform V9 density: dropout clusters follow the tape
        // wear field and the bottom of the frame, instead of becoming a flat
        // V8-style static layer.
        float density = TAPE_DROPOUT * smoothstep(0.25, 0.80, wear) * (0.4 + 0.6 * (1.0 - uv.y));
        float baseDropout = TAPE_DROPOUT * (TAPE_DROPOUT_BASE
            + TAPE_DROPOUT_WEAR * smoothstep(0.30, 1.0, TAPE_WEAR))
            * step(0.005, TAPE_WEAR) * DECK_NOISE;
        density = max(density, baseDropout);
        density = max(density, Tape_scratch(uv));

        // Tracking is deliberately not a hole source here. It already has its
        // own level-and-noise treatment below; feeding it into the hole map
        // saturates the mask and turns the band into a flat white bar.

        // Rows where the frame offset wrapped show the next/previous track's
        // stub, read from the side. Treat the whole band as one dead read.
        float frameY = uv.y + Tape_frameOffset();
        density = max(density, step(1.0, frameY) + step(frameY, 0.0));

        // The dead frame keeps only the dead density: it is one flat field of
        // the same impulses, so the snow has the statistics of a dropout and
        // not a second generator, and no leftover band from the picture.
        density = mix(density, TAPE_DEAD_DENSITY, deadRf);

        uint impulseState = Common_newRngStateEx(fragCoord, 91U);
        float impulse = Common_randomFloat(impulseState);
        fragColor.x = step(1.0 - clamp(density, 0.0, 1.0), impulse);

        uint rngI = Common_newRngState(cell + vec2(17.0, 0.0));
        uint rngQ = Common_newRngState(cell + vec2(43.0, 0.0));
        fragColor.y = 2.0 * Common_randomFloat(rngI) - 1.0;
        fragColor.z = 2.0 * Common_randomFloat(rngQ) - 1.0;

        // The DOC has nothing to compensate with when there is no RF at all.
        // The flag rides in the mask's alpha so the RF field is evaluated
        // once, here, instead of again at full resolution in the consumer.
        fragColor.w = deadRf;
    }
]], base_wide / 2, base_tall, { [0] = tape_mask })

-- Each impulse gets a random run length. The accumulation is a horizontal
-- smear to the left, so the visible result is a bright head and a decaying
-- tail to the right; a real DOC would hold one whole line instead.
local vhs_dropout_blur = make_pass(common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord.xy / iResolution.xy;
        vec2 pixel = 1.0 / iResolution.xy;
        float wear = texture(iChannel1, uv).x;

        uint lenState = Common_newRngStateEx(fragCoord, 80U);
        float lenRand = Common_randomFloat(lenState);

        fragColor = vec4(0.0, 0.0, 0.0, 1.0);

        int steps = int(max(float(VHS_DROPOUT_MIN_STEPS), VHS_DROPOUT_BLUR_STEPS * lenRand));

        for(int i = 0; i < steps; ++i) {
            vec2 uvmod = vec2(uv.x - pixel.x * float(i), uv.y);
            vec4 sample = texture(iChannel0, uvmod);
            fragColor += sample / (0.125 * float(i + 1));
        }

        fragColor.x = clamp(fragColor.x, 0.0, 1.0);
        fragColor.yz = clamp(fragColor.yz, -1.0, 1.0);

        // The dead-read flag is per frame, so it passes through unblurred;
        // the accumulation above would otherwise sum it like a hole.
        fragColor.w = texture(iChannel0, uv).w;
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

-- Chroma path: VHS delays colour relative to luma, loses vertical chroma
-- detail, and occasionally drops colour from a whole line. This pass works on
-- decoded YIQ before the dropout/DOC layer.
local vhs_chroma = make_pass(common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord / iResolution.xy;
        vec2 pixel = 1.0 / iResolution.xy;
        vec4 color = texture(iChannel0, uv);

        vec2 chromaUV = uv + vec2(CHROMA_DELAY_H * pixel.x, CHROMA_DELAY_V * 2.0 * pixel.y);
        chromaUV = clamp(chromaUV, 0.5 * pixel, 1.0 - 0.5 * pixel);
        vec2 delayed = texture(iChannel0, chromaUV).yz;

        vec2 previousField = texture(iChannel0, uv + vec2(0.0, 2.0 * pixel.y)).yz;
        vec2 chroma = mix(delayed, previousField, CHROMA_BLEND);

        float row = floor(uv.y * TAPE_ROWS_PER_FIELD);
        uint lossState = Common_newRngStateEx(vec2(0.0, row), 95U);
        if(Common_randomFloat(lossState) < CHROMA_LOSS)
            chroma = vec2(0.0);

        // Color killer. It sits on the decoder's chroma output, before the
        // DOC layer, so the picture loses colour while the dropout noise
        // below keeps its own: on a real deck the killer mutes the picture
        // chroma, it does not silence the head amplifier.
        chroma *= mix(0.03, 1.0, Tape_colorKiller());

        fragColor = vec4(color.x, chroma, 1.0);
    }
]], base_wide, base_tall, { [0] = vhs_upsample })

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

        vec2 grainP = uv * iResolution.xy * GRAIN_FREQUENCY;
        float grain = (Common_fbm(grainP, DECK_SEED + uint(iFrame) + uint(Common_field(uv)), GRAIN_DETAIL, 0.5) - 0.5)
            * TAPE_GRAIN * (0.15 + 0.85 * wear) * DECK_NOISE;

        // A dropout is a lost signal, so its luma goes to the snow level. The
        // level is not 1 on purpose: a bar at full white reads as a blown
        // highlight, not as noise, and the hole map already carries the shape.
        // The per-pixel jitter keeps the band grainy instead of flat.
        float snow = VHS_SNOW_LEVEL * (0.70 + 0.60 * n1);

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

        // Add the decaying mask to luma, like V8: one impulse makes a bright
        // head and a tail that fades to the right.
        result.x = min(1.0, result.x + mask * bandDead * VHS_SNOW_LEVEL);
        result.yz = mix(result.yz, vec2(n2, n3) * 1.2, streak * (1.0 - 0.90 * band) * TAPE_SPARKLE);

        // A scratch takes the whole row down, so it is snow at full strength.
        result.x = mix(result.x, snow, scratch);
        result.yz = mix(result.yz, vec2(n2, n3) * 0.12, scratch);

        // Mistracking and a crease are read errors, not oxide loss: they
        // take the level and the colour out of their line/band.
        float tracking = Tape_tracking(uv);
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

        // No RF at all: the raster is the dropout signal itself, streaked by
        // the same 64-step blur as any other hole, with the picture gone. The
        // streak shape is the mask, the colour is the same cell noise the DOC
        // uses, so the snow has the statistics of a dropout and not of a
        // second generator.
        float dead = texture(iChannel1, uv).w;
        vec3 lost = vec3(0.05 + 0.95 * mask,
                         0.22 * dropout.y * mask,
                         0.22 * dropout.z * mask);

        fragColor = vec4(mix(result, lost, dead), 1.0);
    }
]], base_wide, base_tall, { [0] = vhs_chroma, [1] = vhs_dropout_blur, [2] = tape_mask })

local vhs_headswitch = make_pass(common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        uint rngStateRow = Common_newRngState(vec2(0.0, fragCoord.y));
        vec2 uv = fragCoord.xy / iResolution.xy;

        // The head-switch zone belongs to the deck. Height and offset are in
        // lines; the old version used a fraction of the frame and had no
        // mid-line start.
        float height = DECK_HEADSWITCH_HEIGHT / TAPE_ROWS;
        float offset = DECK_HEADSWITCH_OFFSET / TAPE_ROWS;
        float visible = max(height - offset, 0.0);

        if(uv.y <= visible && visible > 0.0) {
            float t = (visible - uv.y) / max(visible, 1e-4);
            float shift = DECK_HEADSWITCH_SHIFT * pow(t, DECK_HEADSWITCH_POWER);
            float jiggle = (Common_randomFloat(rngStateRow) * 2.0 - 1.0) * DECK_HEADSWITCH_JITTER * t;
            vec2 uvmod = vec2(uv.x - shift + jiggle, uv.y);

            if((uvmod.x < 0.0) || (uvmod.x > 1.0))
                fragColor = vec4(0.0, 0.0, 0.0, 1.0);
            else fragColor = texture(iChannel0, uvmod);

            fragColor.yz *= clamp(1.0 - 0.9 * pow(t, DECK_HEADSWITCH_POWER), 0.0, 1.0);

            float midlineRow = ceil(visible * TAPE_ROWS) - 1.0;
            if(DECK_HEADSWITCH_MIDLINE > 0.5 && floor(uv.y * TAPE_ROWS) == midlineRow) {
                float midlineJitter = (Common_randomFloat(rngStateRow) * 2.0 - 1.0) * DECK_HEADSWITCH_MIDLINE_JITTER;
                float copyStart = clamp(DECK_HEADSWITCH_MIDLINE_POS + midlineJitter, 0.0, 1.0);
                vec4 unshifted = texture(iChannel0, uv);

                if(uv.x < copyStart)
                    fragColor = unshifted;

                float transverse = (uv.x - copyStart) * iResolution.x / 16.0;
                if(transverse >= 0.0 && transverse < 1.0)
                    fragColor.x += pow(1.0 - transverse, 3.0) * 0.5;
            }
        }
        else {
            float jiggle = 0.5 * Common_randomFloat(rngStateRow) / iResolution.x;
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

-- Tracking: the bottom band and the old scrolling band share one mask. This
-- pass adds horizontal wave, colour loss, transient snow and fine noise.
local vhs_tracking = make_pass(common_glsl .. [[
    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 uv = fragCoord / iResolution.xy;

        // With no RF the band has nothing extra to add: the whole raster is
        // already the noise the band would be showing, and adding it again
        // would just draw a bright stripe across the snow.
        float band = Tape_tracking(uv) * (1.0 - Tape_deadRf());

        if(band <= 0.0) {
            fragColor = texture(iChannel0, uv);
            return;
        }

        uint rngState = Common_newRngState(vec2(0.0, fragCoord.y));
        float wave = (Deck_noise(vec2(uv.y * 30.0, float(iFrame) / 24.0), 92U) * 2.0 - 1.0)
            * DECK_TRACKING_WAVE / iResolution.x;
        vec4 color = texture(iChannel0, uv + vec2(wave * band, 0.0));

        color.yz *= 1.0 - 0.85 * band;
        float ci = Common_randomFloat(rngState);
        float cq = Common_randomFloat(rngState);
        color.yz += vec2(ci, cq) * 0.08 * band;

        float snow = Snow_transient(uv, 91U, DECK_TRACKING_SNOW, DECK_TRACKING_ANISOTROPY);
        color.x += snow * band * (0.2 + 0.8 * clamp(color.x, 0.0, 1.0));
        color.yz += vec2(snow, -snow) * 0.25 * band;

        float fineNoise = Common_noise(uv * iResolution.xy * 0.05, 93U, DECK_SEED + uint(iFrame)) - 0.5;
        color.x += fineNoise * DECK_TRACKING_NOISE * band;

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

