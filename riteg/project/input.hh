// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#pragma once

using InputMode = int;
constexpr static InputMode INPUT_MODE_NULL                  = 0x0000;
constexpr static InputMode INPUT_MODE_DIRECTORY_ITERATOR    = 0x0001;
constexpr static InputMode INPUT_MODE_STD_SPRINTF           = 0x0002;

namespace input
{
extern InputMode mode;
extern std::filesystem::path directory;
extern std::string format;
} // namespace input

namespace input
{
extern GLuint image;
extern int image_width;
extern int image_height;
} // namespace input

namespace input
{
extern std::size_t path_index;
extern std::vector<std::filesystem::path> paths;
} // namespace input

namespace input
{
void reload_image(void);
void update_paths(void);
} // namespace input
