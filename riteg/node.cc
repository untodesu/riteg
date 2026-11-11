#include "riteg/pch.hh"

#include "riteg/node.hh"

#include "riteg/program.hh"
#include "riteg/timings.hh"

namespace
{
constexpr const char* CHANNEL_TIME_UNIFORMS[Node::MAX_CHANNELS] = {
    "iChannelTime[0]",
    "iChannelTime[1]",
    "iChannelTime[2]",
    "iChannelTime[3]",
};

constexpr const char* CHANNEL_RESOLUTION_UNIFORMS[Node::MAX_CHANNELS] = {
    "iChannelResolution[0]",
    "iChannelResolution[1]",
    "iChannelResolution[2]",
    "iChannelResolution[3]",
};

constexpr const char* CHANNEL_UNIFORMS[Node::MAX_CHANNELS] = {
    "iChannel0",
    "iChannel1",
    "iChannel2",
    "iChannel3",
};
} // namespace

Node::Node(Program* program, int width, int height, const TargetKey& target_key, std::string name)
    : m_program(program), m_width(width), m_height(height), m_target_key(target_key), m_name(std::move(name))
{
    assert(m_program != nullptr);
    assert(m_width > 0);
    assert(m_height > 0);
}

Node::~Node(void) = default;

GLuint Node::get_texture(void) const
{
    auto target = get_current_target();
    return target ? target->get_texture() : 0;
}

GLuint Node::get_framebuffer(void) const
{
    auto target = get_current_target();
    return target ? target->get_framebuffer() : 0;
}

int Node::get_texture_width(void) const
{
    return m_width;
}

int Node::get_texture_height(void) const
{
    return m_height;
}

void Node::render(const Timings& timings)
{
    auto target = get_current_target();
    riteg_force_assert_msg(target != nullptr, "node render target is not assigned");

    for(int i = 0; i < MAX_CHANNELS; ++i) {
        auto input = m_inputs[i];

        if(!input)
            continue;

        if(auto dependency = input->as_node())
            riteg_force_assert_msg(dependency->get_current_target() != target, "node samples its own render target");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, target->get_framebuffer());
    glViewport(0, 0, target->get_width(), target->get_height());
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    auto program = m_program->get_id();
    glUseProgram(program);

    glUniform3f(m_program->get_uniform("iResolution"), static_cast<float>(m_width), static_cast<float>(m_height),
        static_cast<float>(m_width) / static_cast<float>(m_height));
    glUniform1f(m_program->get_uniform("iTime"), timings.current_time);
    glUniform1f(m_program->get_uniform("iTimeDelta"), timings.delta_time);
    glUniform1f(m_program->get_uniform("iFrame"), static_cast<float>(timings.frame_count));

    glUniform4f(m_program->get_uniform("iMouse"), 0.0f, 0.0f, 0.0f, 0.0f);
    glUniform4f(m_program->get_uniform("iDate"), 0.0f, 0.0f, 0.0f, 0.0f);

    for(int i = 0; i < MAX_CHANNELS; ++i) {
        auto input = m_inputs[i];

        if(input) {
            auto input_width = input->get_texture_width();
            auto input_height = input->get_texture_height();

            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, input->get_texture());
            glUniform1f(m_program->get_uniform(CHANNEL_TIME_UNIFORMS[i]), timings.current_time);
            glUniform3f(m_program->get_uniform(CHANNEL_RESOLUTION_UNIFORMS[i]), static_cast<float>(input_width),
                static_cast<float>(input_height), static_cast<float>(input_width) / static_cast<float>(input_height));
            glUniform1i(m_program->get_uniform(CHANNEL_UNIFORMS[i]), i);
        }
        else {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, 0);
            glUniform1f(m_program->get_uniform(CHANNEL_TIME_UNIFORMS[i]), 0.0f);
            glUniform3f(m_program->get_uniform(CHANNEL_RESOLUTION_UNIFORMS[i]), 0.0f, 0.0f, 0.0f);
            glUniform1i(m_program->get_uniform(CHANNEL_UNIFORMS[i]), -1);
        }
    }

    for(const auto& [name, value] : m_uniforms) {
        auto location = m_program->get_uniform(name);

        if(location < 0)
            continue;

        switch(value.kind) {
            case UniformValue::Kind::Float:
                glUniform1f(location, value.values[0]);
                break;
            case UniformValue::Kind::Int:
                glUniform1i(location, value.integer);
                break;
            case UniformValue::Kind::Vec2:
                glUniform2f(location, value.values[0], value.values[1]);
                break;
            case UniformValue::Kind::Vec3:
                glUniform3f(location, value.values[0], value.values[1], value.values[2]);
                break;
            case UniformValue::Kind::Vec4:
                glUniform4f(location, value.values[0], value.values[1], value.values[2], value.values[3]);
                break;
        }
    }

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

Node* Node::as_node(void)
{
    return this;
}

Program* Node::get_program(void) const
{
    return m_program;
}

const std::string& Node::get_name(void) const
{
    return m_name;
}

const TargetKey& Node::get_target_key(void) const
{
    return m_target_key;
}

void Node::set_input(int channel, Source* source)
{
    assert(channel >= 0 && channel < MAX_CHANNELS);
    m_inputs[channel] = source;
}

Source* Node::get_input(int channel) const
{
    assert(channel >= 0 && channel < MAX_CHANNELS);
    return m_inputs[channel];
}

void Node::set_uniform(const std::string& name, const UniformValue& value)
{
    auto location = m_program->get_uniform(name);

    if(location < 0 && m_missing_uniforms.find(name) == m_missing_uniforms.end()) {
        LOG_WARNING("node \"{}\": uniform \"{}\" was not found", m_name, name);
        m_missing_uniforms.insert(name);
    }

    m_uniforms[name] = value;
}

void Node::set_target(RenderTarget* target)
{
    m_target = target;
}

void Node::clear_targets(void)
{
    m_target = nullptr;
    m_pair[0] = nullptr;
    m_pair[1] = nullptr;
    m_current = 0;
}

RenderTarget* Node::get_current_target(void) const
{
    if(m_feedback)
        return m_pair[m_current];

    return m_target;
}

bool Node::is_feedback(void) const
{
    return m_feedback;
}

NodePrev* Node::get_prev(void)
{
    if(!m_prev)
        m_prev = std::make_unique<NodePrev>(this);

    m_feedback = true;
    return m_prev.get();
}

void Node::set_target_pair(RenderTarget* front, RenderTarget* back)
{
    m_feedback = true;
    m_pair[0] = front;
    m_pair[1] = back;
    m_current = 0;
}

void Node::swap_targets(void)
{
    m_current = 1 - m_current;
}

RenderTarget* Node::get_back_target(void) const
{
    if(!m_feedback)
        return nullptr;

    return m_pair[1 - m_current];
}

NodePrev::NodePrev(Node* owner) : m_owner(owner)
{
    assert(m_owner != nullptr);
}

GLuint NodePrev::get_texture(void) const
{
    auto target = m_owner->get_back_target();
    return target ? target->get_texture() : 0;
}

GLuint NodePrev::get_framebuffer(void) const
{
    auto target = m_owner->get_back_target();
    return target ? target->get_framebuffer() : 0;
}

int NodePrev::get_texture_width(void) const
{
    return m_owner->get_texture_width();
}

int NodePrev::get_texture_height(void) const
{
    return m_owner->get_texture_height();
}

void NodePrev::render(const Timings& timings)
{
    (void)timings;
    // The previous frame is already stored in the owner's back target
}

NodePrev* NodePrev::as_prev(void)
{
    return this;
}

Node* NodePrev::get_owner(void) const
{
    return m_owner;
}
