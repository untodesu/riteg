// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/graph/shader_node.hh"

ShaderNode::~ShaderNode(void)
{

}

bool ShaderNode::render(void)
{
    for(BaseNode *dependency : inputs) {
        if(dependency != nullptr) {
            if(!dependency->render())
                return false;
            continue;
        }
    }

    return true;
}

NodeType ShaderNode::get_type(void) const
{
    return NodeType::SHADER_NODE;
}
