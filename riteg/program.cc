#include "riteg/pch.hh"

#include "riteg/program.hh"

#include "riteg/shader_bits.hh"

namespace
{
GLuint s_vertex_array_object;
GLuint s_vertex_shader;

GLuint compile_shader(GLenum type, const std::vector<const char*>& sources, std::string& error)
{
    assert(type == GL_VERTEX_SHADER || type == GL_FRAGMENT_SHADER);
    assert(sources.size() >= 1);

    auto shader = glCreateShader(type);
    glShaderSource(shader, static_cast<GLsizei>(sources.size()), sources.data(), nullptr);
    glCompileShader(shader);

    GLint info_log_length;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_length);

    std::string info_log;

    if(info_log_length > 1) {
        info_log.resize(info_log_length);

        GLsizei written = 0;
        glGetShaderInfoLog(shader, info_log_length, &written, info_log.data());
        info_log.resize(written);

        LOG_INFO("shader information: {}", info_log);
    }

    GLint compile_status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);

    if(compile_status == GL_FALSE) {
        error = std::format("shader compilation failed: {}", info_log);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

std::string read_program_log(GLuint program)
{
    GLint info_log_length;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length);

    if(info_log_length <= 1)
        return {};

    std::string info_log(info_log_length, '\0');

    GLsizei written = 0;
    glGetProgramInfoLog(program, info_log_length, &written, info_log.data());
    info_log.resize(written);
    return info_log;
}
} // namespace

void Program::init(void)
{
    std::string error;
    s_vertex_shader = compile_shader(GL_VERTEX_SHADER, { GLSL_VERSION_SHADER_BIT, VERT_ENTRYPOINT_SHADER_BIT }, error);

    if(!s_vertex_shader) {
        LOG_CRITICAL("vertex shader compilation failed: {}", error);
        std::terminate();
    }

    glGenVertexArrays(1, &s_vertex_array_object);
}

void Program::deinit(void)
{
    glDeleteVertexArrays(1, &s_vertex_array_object);
    glDeleteShader(s_vertex_shader);
}

void Program::beginFrame(void)
{
    glBindVertexArray(s_vertex_array_object);
}

std::unique_ptr<Program> Program::compile(const std::string& source, std::string& error)
{
    error.clear();

    std::vector<const char*> sources;
    sources.emplace_back(GLSL_VERSION_SHADER_BIT);
    sources.emplace_back(FRAG_RITEG_SHADER_BIT);
    sources.emplace_back(FRAG_SHADERTOY_HEADER_SHADER_BIT);
    sources.emplace_back(source.c_str());
    sources.emplace_back(FRAG_SHADERTOY_FOOTER_SHADER_BIT);

    std::string shader_error;
    auto fragment_shader = compile_shader(GL_FRAGMENT_SHADER, sources, shader_error);

    if(!fragment_shader) {
        error = std::move(shader_error);
        return nullptr;
    }

    auto program = glCreateProgram();
    glAttachShader(program, s_vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    auto info_log = read_program_log(program);

    if(!info_log.empty())
        LOG_INFO("program information: {}", info_log);

    GLint link_status;
    glGetProgramiv(program, GL_LINK_STATUS, &link_status);

    if(link_status == GL_FALSE) {
        error = std::format("program linking failed: {}", info_log);
        glDeleteProgram(program);
        glDeleteShader(fragment_shader);
        return nullptr;
    }

    auto result = std::unique_ptr<Program>(new Program());
    result->m_program = program;
    result->m_fragment_shader = fragment_shader;
    result->m_source = source;

    return result;
}

Program::~Program(void)
{
    if(m_program)
        glDeleteProgram(m_program);
    if(m_fragment_shader)
        glDeleteShader(m_fragment_shader);
}

GLuint Program::get_id(void) const
{
    return m_program;
}

const std::string& Program::get_source(void) const
{
    return m_source;
}

const std::string& Program::get_name(void) const
{
    return m_name;
}

void Program::set_name(std::string name)
{
    m_name = std::move(name);
}

GLint Program::get_uniform(const std::string& name)
{
    auto found = m_uniforms.find(name);

    if(found != m_uniforms.end())
        return found->second;

    auto location = m_program ? glGetUniformLocation(m_program, name.c_str()) : -1;
    m_uniforms.emplace(name, location);
    return location;
}
