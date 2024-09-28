// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/pch.hh"
#include "riteg/globals.hh"
#include "riteg/node_editor.hh"

void node_editor::init(void)
{

}

void node_editor::render(void)
{
    if(ImGui::Begin("Node editor")) {
        ImNodes::Ez::BeginCanvas();

        struct TestNode {
            ImVec2 pos{};
            bool selected{};
            ImNodes::Ez::SlotInfo inputs[1];
            ImNodes::Ez::SlotInfo outputs[1];
        };

        static TestNode nodes[3] = {
            {{50, 100}, false, {{"In", 1}}, {{"Out", 1}}},
            {{250, 50}, false, {{"In", 1}}, {{"Out", 1}}},
            {{250, 100}, false, {{"In", 1}}, {{"Out", 1}}},
        };

        for(TestNode &node : nodes) {
            if(ImNodes::Ez::BeginNode(&node, "Node Title", &node.pos, &node.selected)) {
                ImNodes::Ez::InputSlots(node.inputs, 1);
                ImNodes::Ez::OutputSlots(node.outputs, 1);
                ImNodes::Ez::EndNode();
            }
        }

        //ImNodes::Connection(&nodes[1], "In", &nodes[0], "Out");
        //ImNodes::Connection(&nodes[2], "In", &nodes[0], "Out");

        ImNodes::Ez::EndCanvas();
    }

    ImGui::End();
}
