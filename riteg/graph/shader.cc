// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/core/logging.hh"
#include "riteg/graph/shader.hh"

ShaderNode::ShaderNode(const std::string &title) : Node(title), params()
{

}

ShaderNode::~ShaderNode(void)
{

}

unsigned int ShaderNode::get_type(void) const
{
    return Node::SHADER;
}

bool ShaderNode::render(void)
{
    for(std::size_t i = 0; i < inputs.size(); ++i) {
        if(inputs[i] != nullptr) {
            if(inputs[i]->render())
                continue;
            return false;
        }
    }

    logging::info("ShaderNode::render: %s", title.c_str());

    return true;
}
