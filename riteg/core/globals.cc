// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/core/globals.hh"

GLFWwindow *globals::window = nullptr;
ImGuiID globals::dockspace_id = {};
ImNodes::Ez::Context *globals::imnodes_ctx = nullptr;

GLuint globals::vertex_shader = 0;
GLuint globals::vertex_array = 0;

std::unordered_set<BaseNode *> globals::render_list = {};
