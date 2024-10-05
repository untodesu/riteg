// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/core/globals.hh"
#include "riteg/gui/menu_bar.hh"
#include "riteg/project/input.hh"
#include "riteg/project/project.hh"

static void layout_file_menu(void)
{
    ImGui::MenuItem("New", nullptr);
    ImGui::MenuItem("Open", nullptr);
    ImGui::Separator();

    if(ImGui::MenuItem("Save", nullptr)) {
        project::save();
    }

    ImGui::MenuItem("Save As", nullptr);
    ImGui::Separator();

    ImGui::MenuItem("Options", nullptr);
    ImGui::Separator();

    if(ImGui::MenuItem("Exit", nullptr)) {
        glfwSetWindowShouldClose(globals::window, true);
    }
}

static void layout_view_menu(void)
{
    if(ImGui::MenuItem("Restore layout")) {
        project::restore_layout();
    }
}

void menu_bar::layout(void)
{
    if(ImGui::BeginMainMenuBar()) {
        if(ImGui::BeginMenu("File")) {
            layout_file_menu();
            ImGui::EndMenu();
        }

        if(ImGui::BeginMenu("View")) {
            layout_view_menu();
            ImGui::EndMenu();
        }

        const float phase = static_cast<float>(glfwGetTime());
        const float col_r = 0.5f + 0.5f * std::cos(phase);
        const float col_b = 0.5f + 0.5f * std::sin(phase);

        const ImGuiIO &io = ImGui::GetIO();
        const ImVec2 &vsz = ImGui::GetMainViewport()->Size;
        ImGui::Separator(); ImGui::TextDisabled("%.0fx%.0f % 3.0f FPS", vsz.x, vsz.y, io.Framerate);
        ImGui::Separator(); ImGui::TextDisabled("FRAME % 5zu/%-5zu", input::path_index + 1, input::paths.size());
        ImGui::Separator(); ImGui::TextColored(ImVec4(col_r, 1.0f, col_b, 1.0f), "RITEG V2");

        ImGui::EndMainMenuBar();
    }
}
