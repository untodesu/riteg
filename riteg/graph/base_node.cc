// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/graph/base_node.hh"

BaseNode::~BaseNode(void)
{
    for(BaseNode *input : inputs) {
        if(input == nullptr)
            continue;
        input->outputs.erase(this);
    }

    for(BaseNode *output : outputs) {
        for(std::size_t i = 0; i < output->inputs.size(); ++i) {
            if(output->inputs[i] == this) {
                output->inputs[i] = nullptr;
                continue;
            }
        }
    }
}
