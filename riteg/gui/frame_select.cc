// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/gui/frame_select.hh"
#include "riteg/project/input.hh"
#include "riteg/project/project.hh"

void frame_select::layout(void)
{
    if(!ImGui::Begin("Frame Select###FrameSelect_Window")) {
        ImGui::End();
        return;
    }

    if(ImGui::BeginListBox("###FrameSelect_ListBox", ImVec2(-1.0f, -1.0f))) {
        for(std::size_t i = 0; i < input::paths.size(); ++i) {
            bool is_selected = (i == input::path_index);

            const auto path = std::filesystem::relative(input::paths[i], project::directory);
            if(ImGui::Selectable(path.generic_string().c_str(), &is_selected)) {
                input::path_index = i;
                input::reload_image();
            }
        }

        ImGui::EndListBox();
    }

    ImGui::End();
}
