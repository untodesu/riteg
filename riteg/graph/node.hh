// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#pragma once

class Node {
public:
    constexpr static unsigned int SOURCE = 0x0000U; // Defines exactly one output
    constexpr static unsigned int TARGET = 0x0001U; // Defines exactly one input
    constexpr static unsigned int SHADER = 0x0002U; // Defines multiple inputs and one output

public:
    explicit Node(void) = delete;
    explicit Node(const std::string &title);
    virtual ~Node(void);

    virtual unsigned int get_type(void) const = 0;

    // This method must ensure dependency nodes
    // aka inputs are rendered successfully before
    // the current node's rendering is done
    virtual bool render(void) = 0;

public:
    std::string title {};
    std::vector<Node *> inputs {};
    std::unordered_set<Node *> outputs {};

public:
    ImVec2 position {};
    bool selected {};
};
