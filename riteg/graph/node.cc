// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/graph/node.hh"

Node::Node(const std::string &title, std::uint64_t id) : title(title), id(id), inputs(), outputs()
{
    texture_width = 0;
    texture_height = 0;
    texture = 0;
}

Node::~Node(void)
{
    for(Node *input : inputs) {
        if(input == nullptr)
            continue;
        input->outputs.erase(this);
    }

    for(Node *output : outputs) {
        for(std::size_t i = 0; i < output->inputs.size(); ++i) {
            if(output->inputs[i] == this) {
                output->inputs[i] = nullptr;
                continue;
            }
        }
    }
}
