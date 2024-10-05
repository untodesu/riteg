// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#pragma once
#include "riteg/graph/base_node.hh"

// This provides a drop-in support for most shaders
// designed to run in browser at https://www.shadertoy.com/
class ShadertoyNode final : public BaseNode {
public:
    explicit ShadertoyNode(void);
    virtual ~ShadertoyNode(void);
    virtual NodeType get_type(void) const override;
    virtual bool render(void) override;

public:
    void update_shader(void);
    void update_texture(void);
    void update_uniforms(void);

public:
    std::string shader_path {};
    std::basic_string<GLchar> shader_info_log {};
    std::basic_string<GLchar> program_info_log {};
    GLuint program {};
    GLuint fbo {};

public:
    GLint u_iChannel0 {};           // uniform sampler2D iChannel0;
    GLint u_iChannel1 {};           // uniform sampler2D iChannel1;
    GLint u_iChannel2 {};           // uniform sampler2D iChannel2;
    GLint u_iChannel3 {};           // uniform sampler2D iChannel3;
    GLint u_iResolution {};         // uniform vec3 iResolution;
    GLint u_iTime {};               // uniform float iTime;
    GLint u_iTimeDelta {};          // uniform float iTimeDelta;
    GLint u_iFrame {};              // uniform int iFrame;
    GLint u_iChannelTime {};        // uniform float iChannelTime[4];
    GLint u_iChannelResolution {};  // uniform vec3 iChannelResolution[4];
    GLint u_iMouse {};              // uniform vec4 iMouse;
};
