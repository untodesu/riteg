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

bool BaseNode::trace_backwards(const BaseNode *target) const
{
    if(this != target) {
        for(BaseNode *input : inputs) {
            if(input == nullptr || !input->trace_backwards(target))
                continue;
            return true;
        }

        return false;
    }

    return true;
}

bool BaseNode::trace_forwards(const BaseNode *target) const
{
    if(this != target) {
        for(BaseNode *output : outputs) {
            if(!output->trace_forwards(target))
                continue;
            return true;
        }

        return false;
    }

    return true;
}

bool BaseNode::trace_path(const BaseNode *target) const
{
    return trace_backwards(target) || trace_forwards(target);
}
