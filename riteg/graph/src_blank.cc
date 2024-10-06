// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/graph/src_blank.hh"

constexpr static int TEXTURE_WIDTH = 2;
constexpr static int TEXTURE_HEIGHT = 2;

NodeType SrcBlankNode::get_type(void) const
{
    return NODE_SRC_BLANK;
}

bool SrcBlankNode::render(void)
{
    if(rendered) {
        return false;
    }

    if(!texture) {
        texture_width = TEXTURE_WIDTH;
        texture_height = TEXTURE_HEIGHT;

        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    const ImU32 color32 = ImGui::GetColorU32(color);
    const ImU32 pixels[TEXTURE_WIDTH * TEXTURE_HEIGHT] = {color32, color32, color32, color32};

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    rendered = true;
    return true;
}
