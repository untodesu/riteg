// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/core/logging.hh"
#include "riteg/graph/shader_pass.hh"

ShaderPassNode::ShaderPassNode(const std::string &title, std::size_t id) : Node(title, id), params()
{

}

ShaderPassNode::~ShaderPassNode(void)
{

}

unsigned int ShaderPassNode::get_type(void) const
{
    return Node::SHADER_PASS;
}

bool ShaderPassNode::render(void)
{
    for(std::size_t i = 0; i < inputs.size(); ++i) {
        if(inputs[i] != nullptr) {
            if(inputs[i]->render())
                continue;
            return false;
        }
    }

    logging::info("ShaderPassNode::render: %s", title.c_str());

    return true;
}
