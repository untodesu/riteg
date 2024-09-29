// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/core/globals.hh"

GLFWwindow *globals::window = nullptr;
ImNodes::Ez::Context *globals::imnodes_ctx = nullptr;
std::mt19937_64 globals::random_dev = {};

std::unordered_set<Node *> globals::pr_nodes = {};
std::size_t globals::pr_num_frames = SIZE_MAX;
std::size_t globals::pr_cur_frame = SIZE_MAX;
