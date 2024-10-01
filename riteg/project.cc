// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/core/globals.hh"
#include "riteg/core/logging.hh"
#include "riteg/graph/base_node.hh"
#include "riteg/project.hh"

std::string project::imgui_ini_path = {};
std::random_device project::random_dev = {};
std::filesystem::path project::directory = {};
std::unordered_set<BaseNode *> project::tree = {};

void project::open(const std::filesystem::path &directory)
{
    if(std::filesystem::is_directory(directory)) {
        project::directory = directory;
        project::tree.clear();

        project::imgui_ini_path = project::directory / "imgui.ini";
        ImGui::GetIO().IniFilename = project::imgui_ini_path.c_str();

        logging::info("project::open: opened %s", directory.string().c_str());
        return;
    }

    project::directory = std::filesystem::path();
    project::tree.clear();

    logging::warn("project::open: %s is not a directory", directory.string().c_str());
}

void project::close(void)
{
    project::directory = std::filesystem::path();
    project::tree.clear();
}

void project::save(void)
{
    if(project::directory.empty()) {
        logging::warn("project:save: no project open");
        return;
    }

    logging::info("project:save: saved %s", project::directory.string().c_str());
    return;
}

void project::restore_layout(void)
{
    ImGui::DockBuilderRemoveNodeChildNodes(globals::dockspace_id);

    ImGuiID left = ImGui::DockBuilderSplitNode(globals::dockspace_id, ImGuiDir_Left, 0.20f, nullptr, &globals::dockspace_id);
    ImGuiID left_down = ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.50f, nullptr, &left);

    // FIXME: these windows are displayed merely as a test case; in future
    // these must be replaced with a project editor and an input/output display window
    ImGui::DockBuilderDockWindow("###StyleEdit_Window", left);
    ImGui::DockBuilderDockWindow("Dear ImGui Demo", left_down);

    ImGui::DockBuilderDockWindow("###NodeEdit_Window", globals::dockspace_id);
    ImGui::DockBuilderFinish(globals::dockspace_id);
}
