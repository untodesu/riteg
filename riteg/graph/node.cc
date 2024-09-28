// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/graph/node.hh"

Node::Node(const std::string &title) : title(title), inputs(), outputs()
{

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
