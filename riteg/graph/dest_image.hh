// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#pragma once
#include "riteg/graph/base_node.hh"

class DestImageNode final : public BaseNode {
public:
    explicit DestImageNode(void);
    virtual ~DestImageNode(void) = default;
    virtual NodeType get_type(void) const override;
    virtual bool render(void) override;

public:
    GLuint fbo {};
    std::vector<stbi_uc> pixels {};
};
