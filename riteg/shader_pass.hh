#ifndef RITEG_SHADER_PASS_HH
#define RITEG_SHADER_PASS_HH 1
#pragma once

#include "riteg/basic_image.hh"

class Timings;

class ShaderPass final {
public:
    inline virtual ~ShaderPass(void)
    {
        glDeleteProgram(program_handle);
        glDeleteFramebuffers(1, &framebuffer);
        glDeleteTextures(1, &output_image.texture);
    }

public:
    std::string name;
    GLuint framebuffer;
    BasicImage output_image;
    std::array<const BasicImage*, MAX_SHADER_CHANNELS> channels;
    std::array<float, MAX_SHADER_PARAMETERS> parameters;
    GLuint program_handle;
    struct {
        GLint iResolution;
        GLint iTime;
        GLint iTimeDelta;
        GLint iFrameRate;
        GLint iFrame;
        GLint iChannelTime;
        GLint iChannelResolution;
        GLint iMouse;
        GLint iChannel0;
        GLint iChannel1;
        GLint iChannel2;
        GLint iChannel3;
        GLint iDate;
        GLint iParam;
    } uniforms;

public:
    static void global_init(void);
    static void global_deinit(void);

public:
    static void create(ShaderPass *pass, const char *name, int width, int height, const char *fshader_filename);
    static void render(const ShaderPass *pass, const Timings &timings);
};

#endif /* RITEG_SHADER_PASS_HH */
