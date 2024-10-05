// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/core/globals.hh"
#include "riteg/core/logging.hh"
#include "riteg/graph/base_node.hh"
#include "riteg/graph/dest_image.hh"
#include "riteg/project/project.hh"

std::filesystem::path project::directory = {};
std::filesystem::path project::json_path = {};
std::string project::imgui_ini_path = {};

std::random_device project::random_dev = {};
std::unordered_set<BaseNode *> project::tree = {};
DestImageNode *project::dest_image = nullptr;

void project::open(const std::filesystem::path &directory)
{
    if(std::filesystem::is_directory(directory)) {
        project::directory = directory;
        project::json_path = std::filesystem::path(project::directory / "riteg.json");
        project::imgui_ini_path = std::filesystem::path(project::directory / "imgui.ini").string();
        ImGui::GetIO().IniFilename = project::imgui_ini_path.c_str();

        project::tree.clear();

        logging::info("project::open: opened %s", directory.generic_string().c_str());
        return;
    }

    project::directory = std::filesystem::path();
    project::json_path = std::filesystem::path();
    project::imgui_ini_path = std::string();
    ImGui::GetIO().IniFilename = nullptr;

    project::tree.clear();

    logging::warn("project::open: %s is not a directory", directory.generic_string().c_str());
}

void project::close(void)
{
    project::directory = std::filesystem::path();
    project::json_path = std::filesystem::path();
    project::imgui_ini_path = std::string();
    ImGui::GetIO().IniFilename = nullptr;

    project::tree.clear();
}

void project::save(void)
{
    if(project::directory.empty()) {
        logging::warn("project:save: no project open");
        return;
    }

    logging::info("project:save: saved %s", project::directory.generic_string().c_str());
    return;
}

void project::restore_layout(void)
{
    ImGui::DockBuilderRemoveNodeChildNodes(globals::dockspace_id);

    ImGuiID left = ImGui::DockBuilderSplitNode(globals::dockspace_id, ImGuiDir_Left, 0.20f, nullptr, &globals::dockspace_id);
    ImGuiID left_down = ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.50f, nullptr, &left);

    ImGui::DockBuilderDockWindow("###ProjectEdit_Window", left);
    ImGui::DockBuilderDockWindow("###FrameSelect_Window", left_down);
    ImGui::DockBuilderDockWindow("###NodeEdit_Window", globals::dockspace_id);
    ImGui::DockBuilderFinish(globals::dockspace_id);
}
