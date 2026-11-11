#pragma once

#include "riteg/render_target.hh"
#include "riteg/source.hh"

class Program;
class NodePrev;

struct UniformValue final {
    enum class Kind {
        Float,
        Int,
        Vec2,
        Vec3,
        Vec4,
    };

    Kind kind = Kind::Float;
    float values[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    int integer = 0;
};

class Node final : public Source {
public:
    constexpr static int MAX_CHANNELS = 4;

    Node(Program* program, int width, int height, const TargetKey& target_key, std::string name);
    virtual ~Node(void) override;

    virtual GLuint get_texture(void) const override;
    virtual GLuint get_framebuffer(void) const override;
    virtual int get_texture_width(void) const override;
    virtual int get_texture_height(void) const override;
    virtual void render(const Timings& timings) override;
    virtual Node* as_node(void) override;

    Program* get_program(void) const;
    const std::string& get_name(void) const;
    const TargetKey& get_target_key(void) const;

    void set_input(int channel, Source* source);
    Source* get_input(int channel) const;

    void set_uniform(const std::string& name, const UniformValue& value);

    void set_target(RenderTarget* target);
    void clear_targets(void);
    RenderTarget* get_current_target(void) const;

    bool is_feedback(void) const;
    NodePrev* get_prev(void);

    void set_target_pair(RenderTarget* front, RenderTarget* back);
    void swap_targets(void);
    RenderTarget* get_back_target(void) const;

private:
    Program* m_program;
    int m_width;
    int m_height;
    TargetKey m_target_key;
    std::string m_name;

    Source* m_inputs[MAX_CHANNELS] = { nullptr, nullptr, nullptr, nullptr };
    std::unordered_map<std::string, UniformValue> m_uniforms;
    std::unordered_set<std::string> m_missing_uniforms;

    RenderTarget* m_target = nullptr;
    RenderTarget* m_pair[2] = { nullptr, nullptr };
    int m_current = 0;
    bool m_feedback = false;

    std::unique_ptr<NodePrev> m_prev;
};

class NodePrev final : public Source {
public:
    explicit NodePrev(Node* owner);
    virtual ~NodePrev(void) override = default;

    virtual GLuint get_texture(void) const override;
    virtual GLuint get_framebuffer(void) const override;
    virtual int get_texture_width(void) const override;
    virtual int get_texture_height(void) const override;
    virtual void render(const Timings& timings) override;
    virtual NodePrev* as_prev(void) override;

    Node* get_owner(void) const;

private:
    Node* m_owner;
};
