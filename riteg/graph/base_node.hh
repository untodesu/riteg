// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#pragma once

enum class NodeType {
    SOURCE_NODE,
    TARGET_NODE,
    SHADER_NODE,
};

class BaseNode {
public:
    explicit BaseNode(void) = default;
    virtual ~BaseNode(void) = default;

    // This must resolve all dependency nodes
    // by also calling BaseNode::render on them
    // and only then render the current node
    virtual bool render(void) = 0;

    // This is used for static polymorphism
    virtual NodeType get_type(void) const = 0;

public:
    std::vector<BaseNode *> inputs {};
    std::unordered_set<BaseNode *> outputs {};

public:
    std::string title {};
    ImVec2 position {};
    bool selected {};
};
