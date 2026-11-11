#pragma once

class Source;
struct Timings;

namespace project
{
void init(void);
void deinit(void);
bool finalize(void);
void render(const Timings& timings);
} // namespace project

namespace project
{
bool load_input_RGBA(const std::filesystem::path& path);
} // namespace project

namespace project
{
void run_lua_script(const std::filesystem::path& filename);
} // namespace project

namespace project
{
const Source* get_display_source(void);
const Source* get_output_source(void);
} // namespace project
