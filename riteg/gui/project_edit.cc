// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/gui/project_edit.hh"
#include "riteg/project/input.hh"
#include "riteg/project/project.hh"

static void layout_general(void)
{
    ImGui::InputText("Description###General_Description", &project::description);
}

static void layout_input(void)
{
    ImGui::Combo("Mode###Input_Mode", &input::mode, "disabled\0directory_iterator\0std::sprintf\0");

    ImGui::BeginDisabled(input::mode == INPUT_MODE_NULL);
    std::string input_directory = input::directory.string();
    ImGui::InputText("Directory###Input_Directory", &input_directory);
    input::directory = input_directory;
    ImGui::EndDisabled();

    ImGui::BeginDisabled(input::mode != INPUT_MODE_STD_SPRINTF);
    ImGui::InputText("Format###Input_Format", &input::format);
    ImGui::EndDisabled();

    if(ImGui::Button("Update paths", ImVec2(ImGui::CalcItemWidth(), 0.0f))) {
        input::update_paths();
        input::reload_image();
    }
}

static void layout_output(void)
{
    ImGui::TextDisabled("In-progress!");
}

static void layout_scripts(void)
{
    ImGui::TextDisabled("In-progress!");
}

void project_edit::layout(void)
{
    if(!ImGui::Begin("Project Edit###ProjectEdit_Window")) {
        ImGui::End();
        return;
    }

    ImGui::SeparatorText("General");
    layout_general();
    ImGui::NewLine();

    ImGui::SeparatorText("Input");
    layout_input();
    ImGui::NewLine();

    ImGui::SeparatorText("Output");
    layout_output();
    ImGui::NewLine();

    ImGui::SeparatorText("Scripts");
    layout_scripts();
    ImGui::NewLine();

    ImGui::End();
}
