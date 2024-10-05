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
    glDeleteFramebuffers(1, &fbo);
    glDeleteProgram(program);
}

NodeType GLSLShaderNode::get_type(void) const
{
    return NODE_GLSL_SHADER;
}

bool GLSLShaderNode::render(void)
{
    if(rendered) {
        return true;
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

    if(!fbo)
        glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        logging::warn("GLSLShaderNode::render: Framebuffer is not complete");
        return false;
    }

    glUseProgram(program);
    glUniform2f(u_resolution, texture_width, texture_height);
    glUniform1f(u_time, static_cast<float>(glfwGetTime()));
    glUniform1fv(u_params, params.size(), params.data());

    for(std::size_t i = 0; i < inputs.size(); ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, inputs[i] ? inputs[i]->texture : 0);
        glUniform1i(u_inputs[i], i);
    }

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

    const char *source_cstr = source.c_str();
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &source_cstr, nullptr);
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
        u_inputs.clear();
        u_inputs.resize(inputs.size(), (-1));

        u_resolution = glGetUniformLocation(program, "u_Resolution");
        u_time = glGetUniformLocation(program, "u_Time");
        u_params = glGetUniformLocation(program, "u_Params");

        for(std::size_t i = 0; i < inputs.size(); ++i) {
            std::string name = "u_Input" + std::to_string(i);
            u_inputs[i] = glGetUniformLocation(program, name.c_str());
        }
    }
}
