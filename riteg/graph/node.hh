// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#pragma once

class Node {
public:
    constexpr static unsigned int SOURCE_IMAGE = 0x0000U;
    constexpr static unsigned int SOURCE_EMPTY = 0x0001U;
    constexpr static unsigned int SOURCE_NOISE = 0x0002U;
    constexpr static unsigned int TARGET_IMAGE = 0x0003U;
    constexpr static unsigned int TARGET_IMGUI = 0x0004U;
    constexpr static unsigned int SHADER_PASS = 0x0005U;

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

public:
    int texture_width {};
    int texture_height {};
    GLuint texture {};
};
