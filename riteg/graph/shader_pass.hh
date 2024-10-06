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

public:
    std::string shader_path {};
    std::vector<float> params {};

public:
    std::string info_log_shader {};
    std::string info_log_program {};
    GLuint framebuffer {};
    GLuint program {};

private:
    std::vector<GLint> u_iChannel {};       // uniform sampler2D iChannelX;
    GLint u_iResolution {};                 // uniform vec3 iResolution;
    GLint u_iTime {};                       // uniform float iTime;
    GLint u_iTimeDelta {};                  // uniform float iTimeDelta;
    GLint u_iFrameRate {};                  // uniform float iFrameRate;
    GLint u_iFrame {};                      // uniform int iFrame;
    GLint u_iChannelTime {};                // uniform float iChannelTime[N];
    GLint u_iChannelResolution {};          // uniform vec3 iChannelResolution[N];
    GLint u_iMouse {};                      // uniform vec4 iMouse;
    GLint u_iParam {};                      // uniform float iParam[N];

private:
    std::vector<std::array<float, 3>> channel_resolutions {};
    std::vector<float> channel_times {};
};
