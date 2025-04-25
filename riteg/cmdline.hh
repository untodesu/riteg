#ifndef RITEG_CMDLINE_HH
#define RITEG_CMDLINE_HH 1
#pragma once

namespace cmdline
{
void init(int argc, char **argv);
void init_late(void);
void deinit(void);
} // namespace cmdline

namespace cmdline
{
bool contains(const char *option);
} // namespace cmdline

namespace cmdline
{
const char *get(const char *option, const char *fallback = nullptr);
} // namespace cmdline

#endif /* RITEG_CMDLINE_HH */
