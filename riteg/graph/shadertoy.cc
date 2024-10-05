// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/core/globals.hh"
#include "riteg/core/logging.hh"
#include "riteg/graph/shadertoy.hh"
#include "riteg/project/project.hh"
#include "riteg/project/input.hh"

constexpr static const char *shader_prefix = R"glsl(
    #version 330 core
    out vec4 shadertoy_Output;
    uniform sampler2D iChannel0;
    uniform sampler2D iChannel1;
    uniform sampler2D iChannel2;
    uniform sampler2D iChannel3;
    uniform vec3 iResolution;
    uniform float iTime;
    uniform float iTimeDelta;
    uniform int iFrame;
    uniform float iChannelTime[4];
    uniform vec3 iChannelResolution[4];
    uniform vec4 iMouse;
)glsl";

constexpr static const char *shader_postfix = R"glsl(
    void main(void)
    {
        mainImage(shadertoy_Output, gl_FragCoord.xy);
    }
)glsl";

ShadertoyNode::ShadertoyNode(void)
{
    inputs.resize(4, nullptr);
}

ShadertoyNode::~ShadertoyNode(void)
{
    glDeleteTextures(1, &texture);
    glDeleteFramebuffers(1, &fbo);
    glDeleteProgram(program);
}

NodeType ShadertoyNode::get_type(void) const
{
    return NODE_SHADERTOY;
}

bool ShadertoyNode::render(void)
{
    if(rendered) {
        return true;
    }

    if(inputs[0] && !inputs[0]->render())
        return false;
    if(inputs[1] && !inputs[1]->render())
        return false;
    if(inputs[2] && !inputs[2]->render())
        return false;
    if(inputs[3] && !inputs[3]->render())
        return false;

    if(!texture || !program) {
        // Nothing to render into or nothing
        // to render with; skip rendering this time
        return false;
    }

    if(!fbo)
        glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        logging::warn("ShadertoyNode::render: Framebuffer is not complete");
        return false;
    }

    glUseProgram(program);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputs[0] ? inputs[0]->texture : 0);
    glUniform1i(u_iChannel0, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, inputs[1] ? inputs[1]->texture : 0);
    glUniform1i(u_iChannel1, 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, inputs[2] ? inputs[2]->texture : 0);
    glUniform1i(u_iChannel2, 2);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, inputs[3] ? inputs[3]->texture : 0);
    glUniform1i(u_iChannel3, 3);

    std::array<float, 4> channel_time = {};
    channel_time[0] = static_cast<float>(glfwGetTime());
    channel_time[1] = channel_time[0];
    channel_time[2] = channel_time[0];
    channel_time[3] = channel_time[0];

    std::array<std::array<float, 3>, 4> channel_resolution = {};
    channel_resolution[0].fill(0.0f);
    channel_resolution[1].fill(0.0f);
    channel_resolution[2].fill(0.0f);
    channel_resolution[3].fill(0.0f);

    if(inputs[0] && inputs[0]->texture) {
        channel_resolution[0][0] = inputs[0]->texture_width;
        channel_resolution[0][1] = inputs[0]->texture_height;
    }

    if(inputs[1] && inputs[1]->texture) {
        channel_resolution[1][0] = inputs[1]->texture_width;
        channel_resolution[1][1] = inputs[1]->texture_height;
    }

    if(inputs[2] && inputs[2]->texture) {
        channel_resolution[2][0] = inputs[2]->texture_width;
        channel_resolution[2][1] = inputs[2]->texture_height;
    }

    if(inputs[3] && inputs[3]->texture) {
        channel_resolution[3][0] = inputs[3]->texture_width;
        channel_resolution[3][1] = inputs[3]->texture_height;
    }

    glUniform3f(u_iResolution, texture_width, texture_height, 0.0f);
    glUniform1f(u_iTime, channel_time[0]);
    glUniform1f(u_iTimeDelta, ImGui::GetIO().DeltaTime);
    glUniform1i(u_iFrame, static_cast<int>(input::path_index));
    glUniform1fv(u_iChannelTime, 4, channel_time.data());
    glUniform3fv(u_iChannelResolution, 4, channel_resolution[0].data());
    glUniform4f(u_iMouse, 0.0f, 0.0f, 0.0f, 0.0f);

    glBindVertexArray(globals::vertex_array);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    rendered = true;
    return true;
}

void ShadertoyNode::update_shader(void)
{
    if(shader_path.empty()) {
        // Don't do anything if
        // the shader path is blank
        return;
    }

    if(program) {
        glDeleteProgram(program);
        program = 0;
    }

    std::filesystem::path path = project::directory / shader_path;
    std::ifstream stream = std::ifstream(path);

    if(!stream.is_open()) {
        shader_info_log = "Unable to open shader file";
        program_info_log = std::basic_string<GLchar>(path.string());
        return;
    }

    std::string source = {};
    stream.seekg(0, std::ios::end);
    source.reserve(stream.tellg());
    stream.seekg(0, std::ios::beg);

    source.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    stream.close();

    const char *source_cstr[3] = {shader_prefix, source.c_str(), shader_postfix};
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 3, source_cstr, nullptr);
    glCompileShader(fragment_shader);

    shader_info_log.clear();
    program_info_log.clear();

    GLint shader_info_log_length = {};
    glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &shader_info_log_length);    

    if(shader_info_log_length > 0) {
        shader_info_log.resize(shader_info_log_length);
        glGetShaderInfoLog(fragment_shader, shader_info_log_length, nullptr, shader_info_log.data());
    }

    GLint compile_status = {};
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &compile_status);

    if(!compile_status) {
        glDeleteShader(fragment_shader);
        return;
    }

    program = glCreateProgram();
    glAttachShader(program, globals::vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    glDeleteShader(fragment_shader);

    GLint program_info_log_length = {};
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &program_info_log_length);

    if(program_info_log_length > 0) {
        program_info_log.resize(program_info_log_length);
        glGetProgramInfoLog(program, program_info_log_length, nullptr, program_info_log.data());
    }

    GLint link_status = {};
    glGetProgramiv(program, GL_LINK_STATUS, &link_status);

    if(!link_status) {
        glDeleteProgram(program);
        program = 0;
    }
}

void ShadertoyNode::update_texture(void)
{
    if(!texture)
        glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texture_width, texture_height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void ShadertoyNode::update_uniforms(void)
{
    if(program) {
        u_iChannel0 = glGetUniformLocation(program, "iChannel0");
        u_iChannel1 = glGetUniformLocation(program, "iChannel1");
        u_iChannel2 = glGetUniformLocation(program, "iChannel2");
        u_iChannel3 = glGetUniformLocation(program, "iChannel3");
        u_iResolution = glGetUniformLocation(program, "iResolution");
        u_iTime = glGetUniformLocation(program, "iTime");
        u_iTimeDelta = glGetUniformLocation(program, "iTimeDelta");
        u_iFrame = glGetUniformLocation(program, "iFrame");
        u_iChannelTime = glGetUniformLocation(program, "iChannelTime");
        u_iChannelResolution = glGetUniformLocation(program, "iChannelResolution");
        u_iMouse = glGetUniformLocation(program, "iMouse");
    }
}
