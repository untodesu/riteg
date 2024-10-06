// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#pragma once

class BaseNode;
class DestImageNode;

namespace project
{
extern std::filesystem::path directory;
extern std::filesystem::path json_path;
} // namespace project

namespace project
{
extern std::string description;
} // namespace project

namespace project
{
extern std::random_device random_dev;
extern std::unordered_set<BaseNode *> tree;
extern DestImageNode *dest_image;
} // namespace project

namespace project
{
void create(const std::filesystem::path &directory);
void open(const std::filesystem::path &directory);
void close(void);
void save(void);
} // namespace project

namespace project
{
BaseNode *node_by_id(unsigned long id);
} // namespace project

namespace project
{
void restore_layout(void);
} // namespace project
