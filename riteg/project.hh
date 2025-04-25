#ifndef RITEG_PROJECT_HH
#define RITEG_PROJECT_HH 1
#pragma once

constexpr static int PROJECT_BLANK_SOURCE_ID = 0;
constexpr static int PROJECT_IMAGE_SOURCE_ID = 1;

class Source;
struct Timings;

namespace project
{
void init(void);
void deinit(void);
void render(const Timings &timings);
} // namespace project

namespace project
{
bool load_input_RGBA(const std::filesystem::path &path);
} // namespace project

namespace project
{
void run_lua_script(const char *filename);
} // namespace project

namespace project
{
const Source *get_source(int id);
const Source *get_display_source(void);
const Source *get_output_source(void);
} // namespace project

#endif /* RITEG_PROJECT_HH */
