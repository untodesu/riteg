#include "riteg/pch.hh"

#include "riteg/shader.hh"

#include "riteg/shader_bits.hh"
#include "riteg/timings.hh"

static GLuint s_vertex_array_object;
static GLuint s_vertex_shader;

static GLuint compile_shader(unsigned int type, const std::vector<const char*>& sources)
{
    assert(type == GL_VERTEX_SHADER || type == GL_FRAGMENT_SHADER);
    assert(sources.size() >= 1);

    auto shader = glCreateShader(type);
    glShaderSource(shader, sources.size(), sources.data(), nullptr);
    glCompileShader(shader);

    GLint info_log_length;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_length);

    if(info_log_length > 2) {
        std::basic_string<GLchar> info_log(info_log_length, GLchar(0x00));
        glGetShaderInfoLog(shader, info_log_length, nullptr, info_log.data());
        LOG_INFO("shader information: {}", info_log);
    }

    GLint compile_status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);

    if(compile_status == GL_FALSE) {
        LOG_INFO("shader compilation failed");
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

Shader::Shader(int width, int height, const std::string& source) : m_target_width(width), m_target_height(height)
{
    assert(width > 0);
    assert(height > 0);

    std::vector<const char*> sources;
    sources.emplace_back(GLSL_VERSION_SHADER_BIT);
    sources.emplace_back(FRAG_RITEG_SHADER_BIT);
    sources.emplace_back(FRAG_SHADERTOY_HEADER_SHADER_BIT);
    sources.emplace_back(source.c_str());
    sources.emplace_back(FRAG_SHADERTOY_FOOTER_SHADER_BIT);

    m_fragment_shader = compile_shader(GL_FRAGMENT_SHADER, sources);

    if(!m_fragment_shader) {
        LOG_CRITICAL("fragment shader compilation failed");
        std::terminate();
    }

    m_program = glCreateProgram();
    glAttachShader(m_program, s_vertex_shader);
    glAttachShader(m_program, m_fragment_shader);
    glLinkProgram(m_program);

    GLint info_log_length;
    glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &info_log_length);

    if(info_log_length > 2) {
        std::basic_string<GLchar> info_log(info_log_length, GLchar(0x00));
        glGetProgramInfoLog(m_program, info_log_length, nullptr, info_log.data());
        LOG_INFO("program information: {}", info_log);
    }

    GLint link_status;
    glGetProgramiv(m_program, GL_LINK_STATUS, &link_status);

    if(link_status == GL_FALSE) {
        LOG_CRITICAL("program linking failed");
        std::terminate();
    }

    glGenTextures(1, &m_target_texture);
    glBindTexture(GL_TEXTURE_2D, m_target_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_target_width, m_target_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenFramebuffers(1, &m_target_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_target_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_target_texture, 0);

    riteg_force_assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_channels[CHANNEL0] = nullptr;
    m_channels[CHANNEL1] = nullptr;
    m_channels[CHANNEL2] = nullptr;
    m_channels[CHANNEL3] = nullptr;

    u_iResolution = glGetUniformLocation(m_program, "iResolution");
    u_iTime = glGetUniformLocation(m_program, "iTime");
    u_iTimeDelta = glGetUniformLocation(m_program, "iTimeDelta");
    u_iFrame = glGetUniformLocation(m_program, "iFrame");
    u_iChannelTime[CHANNEL0] = glGetUniformLocation(m_program, "iChannelTime[0]");
    u_iChannelTime[CHANNEL1] = glGetUniformLocation(m_program, "iChannelTime[1]");
    u_iChannelTime[CHANNEL2] = glGetUniformLocation(m_program, "iChannelTime[2]");
    u_iChannelTime[CHANNEL3] = glGetUniformLocation(m_program, "iChannelTime[3]");
    u_iMouse = glGetUniformLocation(m_program, "iMouse");
    u_iDate = glGetUniformLocation(m_program, "iDate");
    u_iChannelResolution[CHANNEL0] = glGetUniformLocation(m_program, "iChannelResolution[0]");
    u_iChannelResolution[CHANNEL1] = glGetUniformLocation(m_program, "iChannelResolution[1]");
    u_iChannelResolution[CHANNEL2] = glGetUniformLocation(m_program, "iChannelResolution[2]");
    u_iChannelResolution[CHANNEL3] = glGetUniformLocation(m_program, "iChannelResolution[3]");
    u_iChannel[CHANNEL0] = glGetUniformLocation(m_program, "iChannel0");
    u_iChannel[CHANNEL1] = glGetUniformLocation(m_program, "iChannel1");
    u_iChannel[CHANNEL2] = glGetUniformLocation(m_program, "iChannel2");
    u_iChannel[CHANNEL3] = glGetUniformLocation(m_program, "iChannel3");
}

Shader::~Shader(void)
{
    glDeleteProgram(m_program);
    glDeleteShader(m_fragment_shader);
    glDeleteTextures(1, &m_target_texture);
    glDeleteFramebuffers(1, &m_target_fbo);
}

GLuint Shader::get_texture(void) const
{
    return m_target_texture;
}

GLuint Shader::get_framebuffer(void) const
{
    return m_target_fbo;
}

int Shader::get_texture_width(void) const
{
    return m_target_width;
}

int Shader::get_texture_height(void) const
{
    return m_target_height;
}

void Shader::render(const Timings& timings)
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_target_fbo);
    glViewport(0, 0, m_target_width, m_target_height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(m_program);
    glUniform3f(u_iResolution, m_target_width, m_target_height, static_cast<float>(m_target_width) / static_cast<float>(m_target_height));
    glUniform1f(u_iTime, timings.current_time);
    glUniform1f(u_iTimeDelta, timings.delta_time);
    glUniform1f(u_iFrame, timings.frame_count);

    glUniform4f(u_iMouse, 0.0f, 0.0f, 0.0f, 0.0f); // Unsupported in RITEG
    glUniform4f(u_iDate, 0.0f, 0.0f, 0.0f, 0.0f);  // Unsupported in RITEG

    for(int i = 0; i < MAX_CHANNELS; ++i) {
        if(m_channels[i]) {
            auto channel_width = m_channels[i]->get_texture_width();
            auto channel_height = m_channels[i]->get_texture_height();

            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, m_channels[i]->get_texture());
            glUniform1f(u_iChannelTime[i], timings.current_time);
            glUniform3f(u_iChannelResolution[i], channel_width, channel_height,
                static_cast<float>(channel_width) / static_cast<float>(channel_height));
            glUniform1i(u_iChannel[i], i);
        }
        else {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, 0);
            glUniform1f(u_iChannelTime[i], 0.0f);
            glUniform3f(u_iChannelResolution[i], 0.0f, 0.0f, 0.0f);
            glUniform1i(u_iChannel[i], -1);
        }
    }

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void Shader::setChannel(int channel, const Source* source)
{
    assert(channel >= 0 && channel < MAX_CHANNELS);

    m_channels[channel] = source;
}

void Shader::init(void)
{
    s_vertex_shader = compile_shader(GL_VERTEX_SHADER, { GLSL_VERSION_SHADER_BIT, VERT_ENTRYPOINT_SHADER_BIT });

    if(!s_vertex_shader) {
        LOG_CRITICAL("vertex shader compilation faield");
        std::terminate();
    }

    glGenVertexArrays(1, &s_vertex_array_object);
}

void Shader::deinit(void)
{
    glDeleteVertexArrays(1, &s_vertex_array_object);
    glDeleteShader(s_vertex_shader);
}

void Shader::beginFrame(void)
{
    glBindVertexArray(s_vertex_array_object);
}
