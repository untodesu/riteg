// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/core/globals.hh"
#include "riteg/core/logging.hh"
#include "riteg/graph/glsl_shader.hh"
#include "riteg/project/project.hh"

GLSLShaderNode::GLSLShaderNode(void)
{
    inputs.resize(1, nullptr);
}

GLSLShaderNode::~GLSLShaderNode(void)
{
    glDeleteTextures(1, &texture);
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteProgram(program);
}

NodeType GLSLShaderNode::get_type(void) const
{
    return NODE_GLSL_SHADER;
}

bool GLSLShaderNode::render(void)
{
    if(rendered) {
        return false;
    }

    for(std::size_t i = 0; i < inputs.size(); ++i) {
        if(inputs[i] != nullptr) {
            if(inputs[i]->render())
                continue;
            return false;
        }
    }

    if(!texture || !program) {
        // Nothing to render into or nothing
        // to render with; skip rendering this time
        return false;
    }

    if(!framebuffer)
        glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        logging::warn("GLSLShaderNode::render: Framebuffer is not complete");
        return false;
    }

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glViewport(0, 0, texture_width, texture_height);
    glClear(GL_COLOR_BUFFER_BIT);

    std::vector<std::array<GLfloat, 2>> resolutions = {};
    std::vector<GLint> samplers = {};

    samplers.resize(inputs.size(), (-1));
    resolutions.resize(inputs.size(), {0.0f, 0.0f});

    for(std::size_t i = 0; i < inputs.size(); ++i) {
        if(inputs[i] != nullptr) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, inputs[i]->texture);
            resolutions[i][0] = static_cast<GLfloat>(inputs[i]->texture_width);
            resolutions[i][1] = static_cast<GLfloat>(inputs[i]->texture_height);
            samplers[i] = static_cast<GLint>(i);
        }
        else {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    glUseProgram(program);
    glUniform1iv(u_iChannel, samplers.size(), samplers.data());
    glUniform2fv(u_iChannelResolution, resolutions.size(), reinterpret_cast<GLfloat *>(resolutions.data()));
    glUniform2f(u_iResolution, static_cast<GLfloat>(texture_width), static_cast<GLfloat>(texture_height));
    glUniform1f(u_iGlfwTime, static_cast<GLfloat>(glfwGetTime()));
    glUniform1fv(u_iParams, parameters.size(), parameters.data());

    glBindVertexArray(globals::vertex_array);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    rendered = true;
    return true;
}

void GLSLShaderNode::update_shader(void)
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
        info_log_shader = "Unable to open shader file";
        info_log_program = std::basic_string<GLchar>(path.string());
        return;
    }

    std::string source = {};
    stream.seekg(0, std::ios::end);
    source.reserve(stream.tellg());
    stream.seekg(0, std::ios::beg);

    source.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    stream.close();

    const char *source_cstr = source.c_str();
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
    }
}

void GLSLShaderNode::update_texture(void)
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

void GLSLShaderNode::update_uniforms(void)
{
    if(program) {
        u_iChannel = glGetUniformLocation(program, "iChannel");
        u_iChannelResolution = glGetUniformLocation(program, "iChannelResolution");
        u_iResolution = glGetUniformLocation(program, "iResolution");
        u_iGlfwTime = glGetUniformLocation(program, "iGlfwTime");
        u_iParams = glGetUniformLocation(program, "iParams");
    }
}
