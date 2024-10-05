// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#pragma once
#include "riteg/graph/base_node.hh"

class ShaderPassNode final : public BaseNode {
public:
    explicit ShaderPassNode(void);
    virtual ~ShaderPassNode(void);
    virtual NodeType get_type(void) const override;
    virtual bool render(void) override;

public:
    void update_shader(void);
    void update_texture(void);
    void update_uniforms(void);

public:
    std::vector<float> params {};

public:
    std::string shader_path {};
    std::basic_string<GLchar> shader_info_log {};
    std::basic_string<GLchar> program_info_log {};
    GLuint program {};
    GLuint fbo {};

public:
    std::vector<GLint> u_inputs {}; // uniform sampler2D u_InputN;
    GLint u_resolution;             // uniform vec2 u_Resolution;
    GLint u_time;                   // uniform float u_Time;
    GLint u_params;                 // uniform float u_Params[N];
};
