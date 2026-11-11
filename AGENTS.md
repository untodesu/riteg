# AGENTS.md

Guidance for coding agents working on RITEG.V2. Human-facing documentation lives in `docs/`;
this file covers how to change the code without breaking the project or wasting the user's time.

## What this project is

RITEG ("Real-time-ish Image Translation enhanced with GLSL") is a small C++20 / OpenGL 3.3
desktop host for Shadertoy-style GLSL pipelines. A Lua script builds a graph of shader passes;
frames are fed in by a `Loader`, rendered, optionally shown in a GLFW window, and optionally
written out by a `Saver`. There is no editor UI yet even though ImGui is linked.

Version context: the current `2.5.x` line describes projects in Lua. V1 used JSON and early
V2 used TOML; those formats are gone, so do not reintroduce them or copy pre-2.5 project
snippets.

- Language/runtime: C++20, C99 for vendored dependencies.
- Graphics: OpenGL 3.3 core via glad, window and input via GLFW.
- Scripting: Lua 5.4 (`riteg.*` API), one Lua file per project.
- Image I/O: stb_image / stb_image_write.
- Build: CMake >= 3.15 (presets need >= 3.25), Ninja or Makefiles recommended.
- CI builds only the documentation (`.github/workflows/documentation.yml`); no compile CI.

## DSH-specific: builds are the user's job

Building this project from inside DSH fails more often than it succeeds. The MSVC/Ninja
environment, GLFW's `FetchContent` network step, the force-included precompiled header,
`<format>` support, and long compiler output all interact badly with a sandboxed agent
session. Do not spend the user's time fighting that.

Rules:

- **Do not run any build or configure command.** This includes `cmake --preset`,
  `cmake -B`, `cmake --build`, `ninja`, `cpack`, and `tools/build-*.sh` / `tools/pack-*.sh`.
- **Do not run `scripts/setup.py`** (or its `setup_clangd.*` / `setup_intellisense.*`
  wrappers). It rewrites machine-local `.vscode/` files and assumes the user already
  configured a build tree.
- When a change needs compilation, **stop and ask the user** to build. Give the exact
  command for their preset, for example:
  ```
  cmake --build --preset common-release
  ```
  and ask for the compiler output. Fix source code from that output; do not retry the build
  yourself.
- Do not claim a build passed unless the user reported it or you verified the produced
  binary. You may check `build/<preset>/riteg/riteg.exe`'s timestamp: if it is older than
  the sources you edited, the binary is stale and any test against it is invalid.
- Do not edit `build/`, `.cache/`, or generated `build/compile_commands.json`. They are
  local artifacts and git-ignored. `build/compile_commands.json` is a copy made by the
  editor setup script for clangd; it goes stale on reconfigure and only the user refreshes it.
- Network access is only needed once, by GLFW's `FetchContent` fallback. Do not "fix" a
  configure failure by editing dependency CMake unless the user asks.

Reference commands for the user (from the repository root):

```
cmake --preset common-debug
cmake --build --preset common-debug
```

or

```
cmake --preset common-release
cmake --build --preset common-release
```

`tools/build-unix.sh`, `tools/build-win32.sh`, and `tools/build-win64.sh` exist but are
convenience wrappers; leave them to the user.

## Testing

There is no unit-test suite. Verification is a visual smoke test: run an existing binary,
produce a frame with a `Loader`/`Saver` pair, then look at the image.

### Preconditions

- The binary already exists, e.g. `build/common-release/riteg/riteg.exe`. Do not build it.
- Its timestamp is newer than the last source edit.
- The machine has a working OpenGL 3.3 context (a desktop session). A headless box cannot
  run it; ask the user to run it there.
- You have a way to view images (in DSH, the `read_image` tool). Without machine vision,
  do not run blind visual tests and do not claim a visual pass; ask the user to look.

### Smoke-test recipe

The application does **not** exit after saving: its main loop runs until the window is
closed. That is expected. Start it in the background, wait for the output file, then kill
the process. Use a one-shot pipeline so the file appears on the first frame.

PowerShell, from the repository root:

```powershell
$root = (Get-Location).Path
$exe  = Join-Path $root "build/common-release/riteg/riteg.exe"
$out  = Join-Path $env:TEMP "riteg-smoke.png"
$log  = Join-Path $env:TEMP "riteg-smoke.stderr.txt"

Remove-Item $out, $log -ErrorAction SilentlyContinue

$proc = Start-Process -FilePath $exe `
    -ArgumentList "project/VHS.V8.lua", "-loader", "oneshot", "-ipath", "media/vhs43.01.png", "-saver", "oneshot", "-opath", $out `
    -WorkingDirectory $root -RedirectStandardError $log -PassThru

$deadline = (Get-Date).AddSeconds(30)
while (-not (Test-Path $out) -and (Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 200
}
if (Test-Path $out) { Start-Sleep -Milliseconds 500 }   # let the writer finish
if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force }

Get-Content $log
Get-Item $out | Select-Object FullName, Length, LastWriteTime
```

Then inspect `$out` with `read_image`. Expected result for the command above: a 640x480
image derived from `media/vhs43.01.png`, visibly processed by the VHS filter (color,
scanline/noise character, no black or uniform frame, correct orientation).

Notes:

- `Start-Process -ArgumentList` joins its items with spaces without adding quotes. If the
  repository or output path contains spaces, embed the quotes manually or the run fails.
- A missing output file plus a non-empty stderr log means the run failed: report the log,
  do not guess. Shader compile/link failures and graph errors are logged as a critical
  message and the process exits with `EXIT_FAILURE`.
- Killing the process after the file exists is fine. Do not add an auto-exit flag or a
  `-quit` option just to make tests easier unless the user asks for it.
- `VHS.V8.lua` seeds its RNG from `os.time()` and derives a shader seed from Lua's RNG, so
  output is not pixel-reproducible. Compare against the documented look, not a golden image.

### What can be tested

- `-loader oneshot -ipath <file>`: input still image (PNG/JPG/BMP/TGA supported by stb).
- `-loader sprintf -iformat <pattern> [-imaxframes N]`: numbered image sequence.
- `-loader fsiter -ipath <dir>`: every regular file in a directory.
- `-saver oneshot -opath <file>`: one output image; format is chosen by extension
  (`.png`, `.jpg`/`.jpeg`, `.bmp`, `.tga`).
- `-saver sprintf -oformat <pattern> [-omaxframes N]`: numbered image sequence.
- Negative checks without vision: missing required arguments terminate with a critical log
  (`invalid argument [ipath]` etc.). This is useful for checking CLI wiring.

The `Saver` reads the output source's framebuffer. `Blank` and `Image` sources have
framebuffer 0, so they can only be inputs; the project script must call `riteg.output()` on
a shader node before a saver can write anything.

For a targeted shader test, put a throwaway `.lua` project in a temporary directory and pass
its path as the first argument. Do not add test-only projects to `project/`.

## Architecture

```
riteg/
  main.cc           GLFW/GLAD init, main loop, loader/saver dispatch, window blit
  project.hh/.cc    Lua state, riteg.* API, userdata wrappers, finalize/render pass
  graph.hh/.cc      DAG ownership, compile to a flat plan, target pool, plan dump
  node.hh/.cc       shader pass instance (Source) and NodePrev feedback source
  program.hh/.cc    compiled GLSL program, uniform locations, fullscreen VAO
  render_target.hh/.cc  texture + FBO + size/format/sampler state
  source.hh         Source interface: texture, framebuffer, size, render()
  blank.hh/.cc      16x16 constant-color texture source (default black)
  image.hh/.cc      stb-loaded image source; uploads RGBA into GL_RGBA16F
  shader_bits.hh    GLSL 330 header/footer, Shadertoy uniforms, fullscreen triangle strip
  loader.hh + loader_{oneshot,sprintf,fsiter}.{hh,cc}
  saver.hh  + saver_{oneshot,sprintf}.{hh,cc}, saver.cc (glReadPixels + stb write)
  cmdline.hh/.cc    two-phase argv parsing
  timings.hh        current_time, delta_time, frame_count
  logging.hh/.cc    sinks and LOG_* macros
  assert.hh         riteg_force_assert / riteg_force_assert_msg (log + std::terminate)
project/            runnable example pipelines (example.lua, VHS.V8.lua)
media/              sample input images used by the docs and smoke tests
docs/               properdocs site sources; published to GitHub Pages
deps/               vendored glad, imgui, lua, stb; GLFW via find_package or FetchContent
tools/              user-facing build/pack shell scripts
scripts/setup.py    editor environment setup (user-run, machine-local output)
```

Frame flow:

1. `main` creates the window and GL context, parses `-loader`/`-saver`, then
   `Program::init()` and `project::init()` create the GL resources and the Lua state.
2. `cmdline::init_late()` executes the project script (first positional argument). The
   script builds a graph with `riteg.program()` / `riteg.node()` and sets display/output
   sources with `riteg.display()` / `riteg.output()`.
3. `project::finalize()` compiles the graph into a flat plan, allocates render targets and
   performs reachability pruning. `main` exits with `EXIT_FAILURE` if the script or the
   graph is invalid.
4. Each frame: `loader->step()` updates the `Image` source, `project::render()` renders the
   plan in order, then the window is blitted and `saver->step()` runs.
5. On window close: project and program teardown, GLFW shutdown. Loader/saver objects are
   currently not deleted on the normal path; do not treat that as a leak you must fix unless
   asked.

Important graph constraints:

- The graph is a DAG; creation order does not matter because `Graph::compile()` sorts it
  topologically. Cycles are rejected unless they go through `node:prev()`.
- Targets are pooled by exact `(width, height, format, filter, wrap)`. Do not reuse a larger
  target for a smaller node: shaders derive texel sizes from `textureSize()` /
  `iResolution`.
- Display and output sources are pinned for the whole frame; the saver and window blit run
  after rendering.
- Nodes not reachable from display/output are not rendered.
- `project::get_display_source()` returns the display source or the output source as a
  fallback. `main` blits both if they exist; the output is what a saver writes.
- `iFrame` is declared `float` in the shader header and incremented once per rendered frame.

## Code conventions

- C++20, `.cc` for implementation and `.hh` for headers, `#pragma once`.
- 4-space indentation, 140-column limit, left-aligned pointers, sorted includes. Run
  `clang-format` only on files you touched if it is available; do not reformat the tree.
- Every `.cc` includes `"riteg/pch.hh"` first. The PCH is also force-included by CMake, and
  `.clangd` hides it from the include cleaner; keep the explicit include anyway.
- Includes are rooted at the repository (`#include "riteg/..."`), not relative.
- Follow the existing naming: `PascalCase` types, `camelCase` methods/functions,
  `m_` members, `s_` file-level statics, `UPPER_SNAKE` constants.
- Logging goes through `LOG_INFO` / `LOG_WARNING` / `LOG_ERROR` / `LOG_CRITICAL` with
  `std::format` syntax. Fatal invariants use `riteg_force_assert` or
  `riteg_force_assert_msg`; they log and call `std::terminate`. The codebase does not use
  exceptions for error handling.
- New GL objects are created in the constructor and deleted in the destructor; follow the
  `Source` contract (`get_texture`, `get_framebuffer`, dimensions, `render`).
- When adding a source, loader, or saver: add the header and `.cc` to
  `riteg/CMakeLists.txt`, register it in `main.cc` (loaders/savers) or `project.cc` (Lua
  API and graph nodes), and update `docs/01.running.md` / `docs/02.project.md` when the
  user-visible CLI or Lua API changes.

## Known sharp edges

These are current behaviors, not necessarily desired ones. Do not "fix" them incidentally;
if a task touches them, verify the intended behavior first.

- `Loader_Sprintf::init()` checks whether the *format* is empty when deciding whether to
  use `-imaxframes`; since an empty format is rejected above, the limit stays `SIZE_MAX`.
  `Saver_Sprintf` does the same check against the limit and behaves as documented.
- `Image` and `Blank` return framebuffer 0; `Saver::write_source_RGBA` asserts a non-zero
  framebuffer, so they cannot be saved directly. `riteg.output()` rejects them earlier.
- Shader compile and graph errors are no longer fatal `std::terminate` paths: they are
  reported to Lua/log and `main` returns `EXIT_FAILURE`. Loader/saver are still not deleted
  on the normal path; the early-error path does delete them.
- Render targets are pooled by exact `(width, height, format, filter, wrap)`. Do not relax
  this without also giving shaders a logical size independent of `textureSize()`.
- `node:prev()` is the only supported feedback edge. A direct self-input is a graph error.

## Commit messages

The history is a solo developer's log rather than Conventional Commits: there are no
`type(scope):` prefixes, and subjects are plain English phrases.

- Subject: imperative, sentence case, no trailing period: `Add ...`, `Fix ...`,
  `Update ...`, `Set ...`, `Make ...`. Aim for 50 characters, stay under 72.
- Say what changed in the subject; put a non-obvious why in the body.
- Body: optional, one blank line after the subject. Prefer `-` bullets wrapped at about
  72 columns, one thought per bullet; short paragraphs are fine too.
- Reference an issue or PR only when one exists (`Closes #42`).
- No Conventional Commits prefixes and no AI attribution trailers.
- One-word historical messages (`Stuff`, `....`) are not a convention to copy, and neither
  is the profanity in a few of them. Recent commits are the better model.

A body in the current style, from `Add CMake presets and editor environments`:

    - common-debug/common-release into build/<preset>, generator
      left to CMake
    - export compile_commands.json for clangd
    - sh/bat/ps1 wrappers, py -3 first because python3/python in
      PATH are often Microsoft Store aliases

## Documentation

- `docs/` is the source for the published site (properdocs + Material). Keep it in sync
  when CLI flags, the Lua API, supported formats, or build requirements change.
- The docs are built only in CI; do not try to render them locally unless the user asks.
- `README`-style introductory material is in `docs/index.md`, not at the repository root.
- This file is for agents; do not mirror it into `docs/`.

## When in doubt

Ask the user rather than guessing, especially about: whether a behavior is a bug or
intended, which preset/platform to target, whether a new CLI flag may change the public
interface, and whether a change is worth rebuilding for. The project is small; a short
question costs less than a wrong build or a stale-binary test.
