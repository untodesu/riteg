#pragma once

namespace cmdline
{
void init(int argc, char** argv);
void init_late(void);
void deinit(void);
} // namespace cmdline

namespace cmdline
{
bool contains(std::string_view option);
} // namespace cmdline

namespace cmdline
{
std::string_view get(std::string_view option, std::string_view fallback = {});
const char* get_cstr(const char* option, const char* fallback = nullptr);
} // namespace cmdline
