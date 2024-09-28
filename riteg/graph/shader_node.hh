// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#pragma once
#include "riteg/graph/base_node.hh"

class ShaderNode final : public BaseNode {
public:
    virtual ~ShaderNode(void);
    virtual bool render(void) override;
    virtual NodeType get_type(void) const override;
};
