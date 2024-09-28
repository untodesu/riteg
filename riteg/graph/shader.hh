// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#pragma once
#include "riteg/graph/node.hh"

class ShaderNode final : public Node {
public:
    explicit ShaderNode(void) = delete;
    explicit ShaderNode(const std::string &title);
    virtual ~ShaderNode(void);

    virtual unsigned int get_type(void) const override;
    virtual bool render(void) override;

public:
    std::vector<float> params {};
};
