// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#pragma once
#include "riteg/graph/base_node.hh"

class DestDisplayNode final : public BaseNode {
public:
    explicit DestDisplayNode(void);
    virtual ~DestDisplayNode(void) = default;
    virtual unsigned int get_type(void) const override;
    virtual bool render(void) override;

public:
    bool always_render {};
};
