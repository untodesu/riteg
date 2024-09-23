// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/pch.hh"
#include "riteg/globals.hh"
#include "riteg/menu_bar.hh"

void menu_bar::render(void)
{
    if(ImGui::BeginMainMenuBar()) {
        if(ImGui::BeginMenu("File")) {
            ImGui::MenuItem("New", nullptr);
            ImGui::MenuItem("Open", nullptr);
            ImGui::Separator();

            ImGui::MenuItem("Save", nullptr);
            ImGui::MenuItem("Save As", nullptr);
            ImGui::Separator();

            ImGui::MenuItem("Options", nullptr);
            ImGui::Separator();

            if(ImGui::MenuItem("Exit", nullptr)) {
                glfwSetWindowShouldClose(g_window, true);
            }

            ImGui::EndMenu();
        }

        if(ImGui::BeginMenu("Edit")) {
            ImGui::EndMenu();
        }

        if(ImGui::BeginMenu("Project")) {
            ImGui::EndMenu();
        }

        const float phase = static_cast<float>(glfwGetTime());
        const float col_r = 0.5f + 0.5f * std::cos(phase);
        const float col_b = 0.5f + 0.5f * std::sin(phase);

        const ImGuiIO &io = ImGui::GetIO();
        const ImVec2 &vsz = ImGui::GetMainViewport()->Size;
        ImGui::Separator(); ImGui::TextDisabled("%.0fx%.0f % 3.0f FPS", vsz.x, vsz.y, io.Framerate);
        ImGui::Separator(); ImGui::TextDisabled("FRAME % 6zu/%-6zu", g_curframe, g_numframes);
        ImGui::Separator(); ImGui::TextColored(ImVec4(col_r, 1.0f, col_b, 1.0f), "RITEG V2");

        ImGui::EndMainMenuBar();
    }
}
