// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#pragma once
#include "riteg/graph/node.hh"

class TargetImGuiNode final : public Node {
public:
    explicit TargetImGuiNode(void) = delete;
    explicit TargetImGuiNode(const std::string &title);
    virtual ~TargetImGuiNode(void) = default;

    virtual unsigned int get_type(void) const override;
    virtual bool render(void) override;
};
