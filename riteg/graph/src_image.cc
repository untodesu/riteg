// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/graph/src_image.hh"
#include "riteg/project/input.hh"

NodeType SrcImageNode::get_type(void) const
{
    return NODE_SRC_IMAGE;
}

bool SrcImageNode::render(void)
{
    texture_width = input::image_width;
    texture_height = input::image_height;
    texture = input::image;
    return true;
}
