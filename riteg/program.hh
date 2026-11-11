#pragma once

class Program final {
public:
    static void init(void);
    static void deinit(void);
    static void beginFrame(void);

    static std::unique_ptr<Program> compile(const std::string& source, std::string& error);

    Program(const Program&) = delete;
    Program& operator=(const Program&) = delete;

    ~Program(void);

    GLuint get_id(void) const;
    const std::string& get_source(void) const;
    const std::string& get_name(void) const;
    void set_name(std::string name);
    GLint get_uniform(const std::string& name);

private:
    Program(void) = default;

    GLuint m_program = 0;
    GLuint m_fragment_shader = 0;
    std::string m_source;
    std::string m_name;
    std::unordered_map<std::string, GLint> m_uniforms;
};
