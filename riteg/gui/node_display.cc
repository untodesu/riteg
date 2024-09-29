// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/core/globals.hh"
#include "riteg/graph/target_imgui.hh"
#include "riteg/gui/node_display.hh"

void node_display::layout(void)
{
    char buffer[256] = {};

    for(Node *node : globals::pr_nodes) {
        if(node->get_type() != Node::TARGET_IMGUI)
            continue;
        snprintf(buffer, sizeof(buffer), "%s###%zu", node->title.c_str(), node->id);

        ImGui::SetNextWindowSize(ImVec2(128.0f, 128.0f), ImGuiCond_Once);

        if(!ImGui::Begin(buffer, nullptr, 0)) {
            ImGui::End();
            continue;
        }

        if(node->inputs[0] && node->inputs[0]->texture) {
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const ImVec2 size = ImGui::GetContentRegionAvail();
            ImTextureID texture = reinterpret_cast<ImTextureID>(node->inputs[0]->texture);
            ImGui::GetWindowDrawList()->AddImage(texture, pos, ImVec2(pos.x + size.x, pos.y + size.y));
        }

        ImGui::End();
    }
}
