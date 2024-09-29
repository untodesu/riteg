// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#pragma once

class Node;

namespace globals
{
extern GLFWwindow *window;
extern ImNodes::Ez::Context *imnodes_ctx;
extern std::random_device random_dev;

extern std::unordered_set<Node *> pr_nodes;
extern std::size_t pr_num_frames;
extern std::size_t pr_cur_frame;
} // namespace globals
