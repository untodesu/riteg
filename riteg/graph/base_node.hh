// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#pragma once

using NodeType = unsigned int;
constexpr static NodeType NODE_SRC_IMAGE    = 0x0000U;
constexpr static NodeType NODE_SRC_BLANK    = 0x0001U;
constexpr static NodeType NODE_SRC_FEEDBACK = 0x0002U;
constexpr static NodeType NODE_DEST_IMAGE   = 0x0003U;
constexpr static NodeType NODE_DEST_DISPLAY = 0x0004U;
constexpr static NodeType NODE_SHADER_PASS  = 0x0005U;

class BaseNode {
public:
    virtual ~BaseNode(void);
    virtual NodeType get_type(void) const = 0;
    virtual bool render(void) = 0;

public:
    unsigned long id {};
    std::vector<BaseNode *> inputs {};
    std::unordered_set<BaseNode *> outputs {};

public:
    std::string name {};
    ImVec2 position {};
    bool selected {};

public:
    bool rendered {};
    int texture_width {};
    int texture_height {};
    GLuint texture {};
};