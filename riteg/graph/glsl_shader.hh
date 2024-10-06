// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#pragma once
#include "riteg/graph/base_node.hh"

class GLSLShaderNode final : public BaseNode {
public:
    explicit GLSLShaderNode(void);
    virtual ~GLSLShaderNode(void);
    virtual NodeType get_type(void) const override;
    virtual bool render(void) override;

public:
    void update_shader(void);
    void update_texture(void);
    void update_uniforms(void);

public:
    std::string shader_path {};
    std::vector<float> parameters {};

public:
    std::basic_string<GLchar> info_log_shader {};
    std::basic_string<GLchar> info_log_program {};
    GLuint framebuffer {};
    GLuint program {};

public:
    GLint u_iChannel {};        // uniform sampler2D iChannel[N]
    GLint u_iChannelResolution; // uniform vec2 iChannelResolution[N]
    GLint u_iResolution;        // uniform vec2 iResolution
    GLint u_iGlfwTime;          // uniform float iGlfwTime
    GLint u_iParams;            // uniform float iParams[N]
};
