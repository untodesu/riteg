// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/pch.hh"
#include "riteg/globals.hh"

GLFWwindow *g_window = nullptr;
std::size_t g_curframe = SIZE_MAX;
std::size_t g_numframes = SIZE_MAX;

ImNodes::Ez::Context *g_nodes_ctx = nullptr;
