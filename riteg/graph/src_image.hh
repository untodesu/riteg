// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#pragma once
#include "riteg/graph/base_node.hh"

class SrcImageNode final : public BaseNode {
public:
    virtual ~SrcImageNode(void) = default;
    virtual NodeType get_type(void) const override;
    virtual bool render(void) override;

public:
    bool enable_display {false};
};
