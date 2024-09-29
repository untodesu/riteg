// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/graph/target_imgui.hh"

TargetImGuiNode::TargetImGuiNode(const std::string &title) : Node(title)
{
    inputs.resize(1, nullptr);
}

unsigned int TargetImGuiNode::get_type(void) const
{
    return Node::TARGET_IMGUI;
}

bool TargetImGuiNode::render(void)
{
    for(std::size_t i = 0; i < inputs.size(); ++i) {
        if(inputs[i] != nullptr) {
            if(inputs[i]->render())
                continue;
            return false;
        }
    }

    return true;
}
