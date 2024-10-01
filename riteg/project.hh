// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#pragma once

class BaseNode;

namespace project
{
extern std::string imgui_ini_path;
extern std::random_device random_dev;
extern std::filesystem::path directory;
extern std::unordered_set<BaseNode *> tree;
} // namespace project

namespace project
{
void open(const std::filesystem::path &directory);
void close(void);
void save(void);
} // namespace project

namespace project
{
void restore_layout(void);
} // namespace project
