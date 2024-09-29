// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#pragma once
#include "riteg/graph/node.hh"

class ShaderPassNode final : public Node {
public:
    explicit ShaderPassNode(void) = delete;
    explicit ShaderPassNode(const std::string &title, std::size_t id);
    virtual ~ShaderPassNode(void);

    virtual unsigned int get_type(void) const override;
    virtual bool render(void) override;

public:
    std::vector<float> params {};
};
