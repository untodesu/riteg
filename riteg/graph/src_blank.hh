// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#pragma once
#include "riteg/graph/base_node.hh"

class SrcBlankNode final : public BaseNode {
public:
    virtual NodeType get_type(void) const override;
    virtual bool render(void) override;

public:
    ImVec4 color {0.0f, 0.0f, 0.0f, 1.0f};
};
