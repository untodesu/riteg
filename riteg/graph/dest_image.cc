// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/core/logging.hh"
#include "riteg/graph/dest_image.hh"

DestImageNode::DestImageNode(void)
{
    inputs.resize(1, nullptr);
}

NodeType DestImageNode::get_type(void) const
{
    return NODE_DEST_IMAGE;
}

bool DestImageNode::render(void)
{
    if(rendered) {
        return false;
    }

    if(inputs.empty() || !inputs[0]->texture) {
        // Nothing to pull data from
        return false;
    }

    for(std::size_t i = 0; i < inputs.size(); ++i) {
        if(inputs[i] != nullptr) {
            if(inputs[i]->render())
                continue;
            return false;
        }
    }

    if(!fbo)
        glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, inputs[0]->texture, 0);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        logging::warn("DestImageNode::render: Framebuffer is not complete");
        return false;
    }

    pixels.resize(inputs[0]->texture_width * inputs[0]->texture_height * 4, stbi_uc(0x00));
    glReadPixels(0, 0, inputs[0]->texture_width, inputs[0]->texture_height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    rendered = true;
    return true;
}
