// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/core/format.hh"
#include "riteg/core/globals.hh"
#include "riteg/core/logging.hh"
#include "riteg/graph/shader_pass.hh"
#include "riteg/project/input.hh"
#include "riteg/project/project.hh"

ShaderPassNode::ShaderPassNode(void)
{
    inputs.resize(4, nullptr);
}

ShaderPassNode::~ShaderPassNode(void)
{
    glDeleteTextures(1, &texture);
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteProgram(program);
}

NodeType ShaderPassNode::get_type(void) const
{
    return NODE_SHADER_PASS;
}

bool ShaderPassNode::render(void)
{
    if(rendered) {
        return true;
    }

    for(std::size_t i = 0; i < inputs.size(); ++i) {
        if(inputs[i] == nullptr)
            continue;
        if(inputs[i]->render())
            continue;
        return false;
    }

    if(!texture || !program) {
        return false;
    }

    if(!framebuffer)
        glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        logging::warn("ShaderPassNode::render: Framebuffer is not complete");
        return false;
    }

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glViewport(0, 0, texture_width, texture_height);
    glClear(GL_COLOR_BUFFER_BIT);

    const float curtime = static_cast<float>(glfwGetTime());
    channel_resolutions.resize(inputs.size(), {0.0f, 0.0f, 0.0f});
    channel_times.resize(inputs.size(), 0.0f);

    for(std::size_t i = 0; i < inputs.size(); ++i) {
        if(inputs[i] == nullptr)
            continue;
        channel_resolutions[i][0] = inputs[i]->texture_width;
        channel_resolutions[i][1] = inputs[i]->texture_height;
        channel_times[i] = curtime;
    }

    glUseProgram(program);
    glUniform3f(u_iResolution, texture_width, texture_height, 0.0f);
    glUniform1f(u_iTime, curtime);
    glUniform1f(u_iTimeDelta, ImGui::GetIO().DeltaTime);
    glUniform1f(u_iFrameRate, ImGui::GetIO().Framerate);
    glUniform1i(u_iFrame, static_cast<GLint>(input::path_index));
    glUniform1fv(u_iChannelTime, channel_times.size(), &channel_times[0]);
    glUniform3fv(u_iChannelResolution, channel_resolutions.size(), &channel_resolutions[0][0]);
    glUniform4f(u_iMouse, 0.0f, 0.0f, 0.0f, 0.0f);
    glUniform1fv(u_iParam, params.size(), &params[0]);

    for(std::size_t i = 0; i < inputs.size(); ++i) {
        glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + i));
        glBindTexture(GL_TEXTURE_2D, inputs[i] ? inputs[i]->texture : 0);
        glUniform1i(u_iChannel[i], static_cast<GLint>(i));
    }

    glBindVertexArray(globals::vertex_array);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    rendered = true;
    return true;
}

void ShaderPassNode::update_shader(void)
{
    if(shader_path.empty()) {
        info_log_shader.clear();
        info_log_program.clear();
        return;
    }

    if(program)
        glDeleteProgram(program);
    program = 0;

    std::filesystem::path path = project::directory / shader_path;
    std::ifstream stream = std::ifstream(path);

    if(!stream.is_open()) {
        info_log_shader = "Failed to open shader file";
        info_log_program = path.generic_string();
        return;
    }

    char buffer[64] = {};
    std::string line = {};
    std::ostringstream source = {};
    source << "#version 330 core"   << std::endl;
    source << "#define RITEG 1"     << std::endl;

    source << "uniform vec3 iResolution;"   << std::endl;
    source << "uniform float iTime;"        << std::endl;
    source << "uniform float iTimeDelta;"   << std::endl;
    source << "uniform float iFrameRate;"   << std::endl;
    source << "uniform int iFrame;"         << std::endl;
    source << "uniform vec4 iMouse;"        << std::endl;

    if(!inputs.empty()) {
        source << util::format("uniform vec3 iChannelResolution[%zu];", inputs.size()) << std::endl;
        source << util::format("uniform float iChannelTime[%zu];", inputs.size()) << std::endl;
    }

    if(!params.empty()) {
        source << util::format("uniform float iParam[%zu];", params.size()) << std::endl;
    }

    for(std::size_t i = 0; i < inputs.size(); ++i) {
        source << util::format("uniform sampler2D iChannel%zu;", i) << std::endl;
    }

    while(std::getline(stream, line))
        source << line << std::endl;
    stream.close();

    source << "out vec4 RITEG_FragColor;"                           << std::endl;
    source << "void main(void) {"                                   << std::endl;
    source << "    mainImage(RITEG_FragColor, gl_FragCoord.xy);"    << std::endl;
    source << "}"                                                   << std::endl;

    const std::string source_str = source.str();
    const char *source_cstr = source_str.c_str();

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &source_cstr, nullptr);
    glCompileShader(fragment_shader);

    info_log_shader.clear();
    info_log_program.clear();

    GLint info_log_length_shader = {};
    glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &info_log_length_shader);    

    if(info_log_length_shader > 0) {
        info_log_shader.resize(info_log_length_shader);
        glGetShaderInfoLog(fragment_shader, info_log_length_shader, nullptr, info_log_shader.data());
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

    GLint info_log_length_program = {};
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length_program);

    if(info_log_length_program > 0) {
        info_log_program.resize(info_log_length_program);
        glGetProgramInfoLog(program, info_log_length_program, nullptr, info_log_program.data());
    }

    GLint link_status = {};
    glGetProgramiv(program, GL_LINK_STATUS, &link_status);

    if(!link_status) {
        glDeleteProgram(program);
        program = 0;
        return;
    }

    u_iChannel.resize(inputs.size(), -1);

    for(std::size_t i = 0; i < inputs.size(); ++i) {
        const std::string name = util::format("iChannel%zu", i);
        u_iChannel[i] = glGetUniformLocation(program, name.c_str());
    }

    u_iResolution = glGetUniformLocation(program, "iResolution");
    u_iTime = glGetUniformLocation(program, "iTime");
    u_iTimeDelta = glGetUniformLocation(program, "iTimeDelta");
    u_iFrameRate = glGetUniformLocation(program, "iFrameRate");
    u_iFrame = glGetUniformLocation(program, "iFrame");
    u_iChannelTime = glGetUniformLocation(program, "iChannelTime");
    u_iChannelResolution = glGetUniformLocation(program, "iChannelResolution");
    u_iMouse = glGetUniformLocation(program, "iMouse");

    u_iParam = glGetUniformLocation(program, "iParam");
}

void ShaderPassNode::update_texture(void)
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
