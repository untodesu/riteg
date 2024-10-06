// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#pragma once

class BaseNode;

namespace globals
{
extern GLFWwindow *window;
extern ImGuiID dockspace_id;
extern ImNodes::Ez::Context *imnodes_ctx;

extern GLuint vertex_shader;
extern GLuint vertex_array;

// Rendering must be done either before or after
// ImGui; this contains all the nodes that are to
// be rendered after the current frame
extern std::unordered_set<BaseNode *> render_list;
} // namespace globals
