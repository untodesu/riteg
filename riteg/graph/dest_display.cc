// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/graph/dest_display.hh"

DestDisplayNode::DestDisplayNode(void)
{
    inputs.resize(1, nullptr);
}

NodeType DestDisplayNode::get_type(void) const
{
    return NODE_DEST_DISPLAY;
}

bool DestDisplayNode::render(void)
{
    if(rendered) {
        return true;
    }

    for(std::size_t i = 0; i < inputs.size(); ++i) {
        if(inputs[i] != nullptr) {
            if(inputs[i]->render())
                continue;
            return false;
        }
    }

    rendered = true;
    return true;
}
