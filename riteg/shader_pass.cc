#include "riteg/precompiled.hh"
#include "riteg/shader_pass.hh"

#include "riteg/debug_out.hh"
#include "riteg/timings.hh"

constexpr static const char *vshader_source = R"glsl(
    #version 330 core

    void main(void)
    {
        vec2 coords[4];
        coords[0] = vec2(-1.0, -1.0);
        coords[1] = vec2(-1.0, +1.0);
        coords[2] = vec2(+1.0, -1.0);
        coords[3] = vec2(+1.0, +1.0);
        gl_Position = vec4(coords[gl_VertexID % 4], 0.0, 1.0);
    }
)glsl";

static GLuint vaobj;
static GLuint vshader;

void ShaderPass::global_init(void)
{
    glGenVertexArrays(1, &vaobj);

    vshader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vshader, 1, &vshader_source, nullptr);
    glCompileShader(vshader);

    GLint status, info_log_length;
    std::basic_string<GLchar> info_log;

    glGetShaderiv(vshader, GL_COMPILE_STATUS, &status);
    glGetShaderiv(vshader, GL_INFO_LOG_LENGTH, &info_log_length);

    if(info_log_length > 2) {
        info_log.resize(info_log_length, GLchar(0x00));
        glGetShaderInfoLog(vshader, info_log.size(), nullptr, info_log.data());
        debug_out << "Vertex Shader:\n" << info_log << "\n";
    }

    if(!status) {
        throw std::runtime_error("shader compilation failed");
    }
}

void ShaderPass::global_deinit(void)
{
    glDeleteShader(vshader);
    glDeleteVertexArrays(1, &vaobj);
}

void ShaderPass::create(ShaderPass *pass, const char *name, int width, int height, const char *fshader_filename)
{
    if(width <= 0) throw std::range_error("pass width is out of range");
    if(height <= 0) throw std::range_error("pass height is out of range");

    pass->channels.fill(nullptr);
    pass->parameters.fill(0.0f);

    pass->name = name;

    pass->output_image.resolution[0] = width;
    pass->output_image.resolution[1] = height;
    pass->output_image.resolution[2] = static_cast<float>(width) / static_cast<float>(height);

    glGenTextures(1, &pass->output_image.texture);
    glBindTexture(GL_TEXTURE_2D, pass->output_image.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glGenFramebuffers(1, &pass->framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, pass->framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pass->output_image.texture, 0);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        throw std::runtime_error("pass framebuffer is incomplete");
    }

    auto fshader_file = std::ifstream(fshader_filename);

    if(fshader_file.fail()) {
        throw std::system_error(errno, std::system_category(), fshader_filename);
    }

    auto raw_source = std::string(std::istreambuf_iterator<char>(fshader_file), std::istreambuf_iterator<char>());
    auto source_stream = std::ostringstream();

    source_stream << "#version 330 core"                            << std::endl;
    source_stream << "#define RITEG 1"                              << std::endl;
    source_stream << "uniform vec3 iResolution;"                    << std::endl;
    source_stream << "uniform float iTime;"                         << std::endl;
    source_stream << "uniform float iTimeDelta;"                    << std::endl;
    source_stream << "uniform float iFrameRate;"                    << std::endl;
    source_stream << "uniform int iFrame;"                          << std::endl;
    source_stream << "uniform float iChannelTime[4];"               << std::endl;
    source_stream << "uniform vec3 iChannelResolution[4];"          << std::endl;
    source_stream << "uniform vec4 iMouse;"                         << std::endl;
    source_stream << "uniform sampler2D iChannel0;"                 << std::endl;
    source_stream << "uniform sampler2D iChannel1;"                 << std::endl;
    source_stream << "uniform sampler2D iChannel2;"                 << std::endl;
    source_stream << "uniform sampler2D iChannel3;"                 << std::endl;
    source_stream << "uniform vec4 iDate;"                          << std::endl;
    source_stream << "uniform float iParam[16];"                    << std::endl;
    source_stream << "// begin shader: " << fshader_filename        << std::endl;
    source_stream << raw_source                                     << std::endl;
    source_stream << "// end shader: " << fshader_filename          << std::endl;
    source_stream << "out vec4 RITEG_fragColor;"                    << std::endl;
    source_stream << "void main(void) {"                            << std::endl;
    source_stream << "mainImage(RITEG_fragColor, gl_FragCoord.xy);" << std::endl;
    source_stream << "}"                                            << std::endl;

    auto source = source_stream.str();
    auto source_cstr = source.c_str();

    auto fshader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fshader, 1, &source_cstr, nullptr);
    glCompileShader(fshader);

    GLint status, info_log_length;
    std::basic_string<GLchar> info_log;

    glGetShaderiv(fshader, GL_COMPILE_STATUS, &status);
    glGetShaderiv(fshader, GL_INFO_LOG_LENGTH, &info_log_length);

    if(info_log_length > 2) {
        info_log.resize(info_log_length, GLchar(0x00));
        glGetShaderInfoLog(fshader, info_log.size(), nullptr, info_log.data());
        debug_out << fshader_filename << ":\n" << info_log << "\n";
    }

    if(!status) {
        throw std::runtime_error("pass shader compilation failed");
    }

    pass->program_handle = glCreateProgram();
    glAttachShader(pass->program_handle, vshader);
    glAttachShader(pass->program_handle, fshader);
    glLinkProgram(pass->program_handle);

    glDeleteShader(fshader);

    glGetProgramiv(pass->program_handle, GL_LINK_STATUS, &status);
    glGetProgramiv(pass->program_handle, GL_INFO_LOG_LENGTH, &info_log_length);

    if(info_log_length > 2) {
        info_log.resize(info_log_length, GLchar(0x00));
        glGetProgramInfoLog(pass->program_handle, info_log.size(), nullptr, info_log.data());
        debug_out << fshader_filename << ":\n" << info_log << "\n";
    }

    if(!status) {
        throw std::runtime_error("pass program linking failed");
    }

    pass->uniforms.iResolution = glGetUniformLocation(pass->program_handle, "iResolution");

    pass->uniforms.iTime = glGetUniformLocation(pass->program_handle, "iTime");
    pass->uniforms.iTimeDelta = glGetUniformLocation(pass->program_handle, "iTimeDelta");

    pass->uniforms.iFrameRate = glGetUniformLocation(pass->program_handle, "iFrameRate");
    pass->uniforms.iFrame = glGetUniformLocation(pass->program_handle, "iFrame");

    pass->uniforms.iChannelTime = glGetUniformLocation(pass->program_handle, "iChannelTime");
    pass->uniforms.iChannelResolution = glGetUniformLocation(pass->program_handle, "iChannelResolution");

    // This is only here to comply with shadertoy spec for uniforms
    // on graphical shader programs; values inside are always zeroed out
    pass->uniforms.iMouse = glGetUniformLocation(pass->program_handle, "iMouse");

    pass->uniforms.iChannel0 = glGetUniformLocation(pass->program_handle, "iChannel0");
    pass->uniforms.iChannel1 = glGetUniformLocation(pass->program_handle, "iChannel1");
    pass->uniforms.iChannel2 = glGetUniformLocation(pass->program_handle, "iChannel2");
    pass->uniforms.iChannel3 = glGetUniformLocation(pass->program_handle, "iChannel3");

    // This is only here to comply with shadertoy spec for uniforms
    // on graphical shader programs; the shader thinks it's 0000-00-00 00:00:00
    pass->uniforms.iDate = glGetUniformLocation(pass->program_handle, "iDate");

    // RITEG extension; because there's no dynamic code editor, it makes sense to
    // allow parameters to be filled from a pipeline setup file, which we're actually doing
    pass->uniforms.iParam = glGetUniformLocation(pass->program_handle, "iParam");

    pass->channels.fill(nullptr);
    pass->parameters.fill(FLT_MAX);
}

void ShaderPass::render(const ShaderPass *pass, const Timings &timings)
{
    glViewport(0, 0, pass->output_image.resolution[0], pass->output_image.resolution[1]);
    glBindFramebuffer(GL_FRAMEBUFFER, pass->framebuffer);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(pass->program_handle);
    glUniform3fv(pass->uniforms.iResolution, 1, pass->output_image.resolution.data());
    glUniform1f(pass->uniforms.iTime, timings.curtime);
    glUniform1f(pass->uniforms.iTimeDelta, timings.frametime);
    glUniform1f(pass->uniforms.iFrameRate, timings.framerate);
    glUniform1i(pass->uniforms.iFrame, timings.frame_number);
    glUniform4f(pass->uniforms.iMouse, 0.0f, 0.0f, 0.0f, 0.0f);
    glUniform1i(pass->uniforms.iChannel0, GLint(0)); // GL_TEXTURE0
    glUniform1i(pass->uniforms.iChannel1, GLint(1)); // GL_TEXTURE1
    glUniform1i(pass->uniforms.iChannel2, GLint(2)); // GL_TEXTURE2
    glUniform1i(pass->uniforms.iChannel3, GLint(3)); // GL_TEXTURE3
    glUniform4f(pass->uniforms.iDate, 0.0f, 0.0f, 0.0f, 0.0f);
    glUniform1fv(pass->uniforms.iParam, MAX_SHADER_PARAMETERS, pass->parameters.data());

    std::array<float, MAX_SHADER_CHANNELS> channel_times;
    std::array<Vec3f, MAX_SHADER_CHANNELS> channel_resolutions;

    for(int i = 0; i < MAX_SHADER_CHANNELS; ++i) {
        glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + i));
        
        if(pass->channels[i] == nullptr) {
            glBindTexture(GL_TEXTURE_2D, GLuint(0));
            channel_resolutions[i].fill(0.0f);
            channel_times[i] = FLT_MAX;
        }
        else {
            glBindTexture(GL_TEXTURE_2D, pass->channels[i]->texture);
            channel_resolutions[i] = pass->channels[i]->resolution;
            channel_times[i] = timings.curtime;
        }
    }

    glUniform1fv(pass->uniforms.iChannelTime, MAX_SHADER_CHANNELS, channel_times.data());
    glUniform3fv(pass->uniforms.iChannelResolution, MAX_SHADER_CHANNELS, channel_resolutions.data()->data());

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}
