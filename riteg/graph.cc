#include "riteg/pch.hh"

#include "riteg/graph.hh"

#include "riteg/node.hh"
#include "riteg/program.hh"
#include "riteg/source.hh"
#include "riteg/timings.hh"

namespace
{
enum class VisitColor {
    White,
    Gray,
    Black,
};

struct CompileState final {
    std::unordered_map<Node*, VisitColor> colors;
    std::vector<Node*> plan;
    std::string error;
};

bool visit_node(Node* node, CompileState& state);

bool visit_input(Source* input, Node* node, CompileState& state)
{
    if(!input)
        return true;

    if(auto dependency = input->as_node()) {
        if(dependency == node) {
            state.error = std::format("graph: node \"{}\" samples itself; use node:prev() for feedback", node->get_name());
            return false;
        }

        return visit_node(dependency, state);
    }

    if(auto previous = input->as_prev()) {
        auto owner = previous->get_owner();

        if(owner != node)
            return visit_node(owner, state);
    }

    return true;
}

bool visit_node(Node* node, CompileState& state)
{
    auto found = state.colors.find(node);
    auto color = found == state.colors.end() ? VisitColor::White : found->second;

    if(color == VisitColor::Black)
        return true;

    if(color == VisitColor::Gray) {
        state.error = std::format("graph: cycle detected at node \"{}\"; use node:prev() for feedback", node->get_name());
        return false;
    }

    state.colors[node] = VisitColor::Gray;

    for(int channel = 0; channel < Node::MAX_CHANNELS; ++channel) {
        if(!visit_input(node->get_input(channel), node, state))
            return false;
    }

    state.colors[node] = VisitColor::Black;
    state.plan.push_back(node);
    return true;
}

const char* format_name(TextureFormat format)
{
    switch(format) {
        case TextureFormat::RGBA16F:
            return "rgba16f";
        case TextureFormat::RGBA8:
            return "rgba8";
        case TextureFormat::R16F:
            return "r16f";
        case TextureFormat::RG16F:
            return "rg16f";
    }

    return "unknown";
}
} // namespace

Graph::Graph(void) = default;

Graph::~Graph(void) = default;

void Graph::clear(void)
{
    for(auto& node : m_nodes)
        node->clear_targets();

    m_plan.clear();
    m_targets.clear();
    m_nodes.clear();
    m_program_cache.clear();
    m_programs.clear();
    m_output = nullptr;
    m_display = nullptr;
    m_compiled = false;
    m_dirty = true;
}

Program* Graph::get_program(const std::string& source, std::string& error)
{
    auto found = m_program_cache.find(source);

    if(found != m_program_cache.end())
        return found->second;

    auto program = Program::compile(source, error);

    if(!program)
        return nullptr;

    auto result = program.get();
    m_programs.push_back(std::move(program));
    m_program_cache.emplace(source, result);
    return result;
}

Node* Graph::create_node(Program* program, int width, int height, const NodeOptions& options)
{
    riteg_force_assert(program != nullptr);
    riteg_force_assert(width > 0);
    riteg_force_assert(height > 0);

    auto resolved = options;

    if(resolved.name.empty())
        resolved.name = std::format("node{}", m_nodes.size());

    TargetKey key { width, height, resolved.format, resolved.filter, resolved.wrap };
    auto node = std::make_unique<Node>(program, width, height, key, resolved.name);
    auto result = node.get();

    m_nodes.push_back(std::move(node));
    m_dirty = true;
    return result;
}

void Graph::connect(Node* node, int channel, Source* source)
{
    riteg_force_assert(node != nullptr);
    riteg_force_assert(channel >= 0);
    riteg_force_assert(channel < Node::MAX_CHANNELS);

    node->set_input(channel, source);
    m_dirty = true;
}

bool Graph::set_output(Source* source, std::string& error)
{
    if(!source) {
        m_output = nullptr;
        m_dirty = true;
        return true;
    }

    auto node = source->as_node();

    if(!node) {
        error = "output source must be a shader node";
        return false;
    }

    auto format = node->get_target_key().format;

    if(format != TextureFormat::RGBA16F && format != TextureFormat::RGBA8) {
        error = std::format("output node \"{}\" must use rgba16f or rgba8 format", node->get_name());
        return false;
    }

    m_output = source;
    m_dirty = true;
    return true;
}

bool Graph::set_display(Source* source, std::string& error)
{
    if(!source) {
        m_display = nullptr;
        m_dirty = true;
        return true;
    }

    auto node = source->as_node();

    if(!node) {
        error = "display source must be a shader node";
        return false;
    }

    auto format = node->get_target_key().format;

    if(format != TextureFormat::RGBA16F && format != TextureFormat::RGBA8) {
        error = std::format("display node \"{}\" must use rgba16f or rgba8 format", node->get_name());
        return false;
    }

    m_display = source;
    m_dirty = true;
    return true;
}

Source* Graph::get_output(void) const
{
    return m_output;
}

Source* Graph::get_display(void) const
{
    return m_display;
}

bool Graph::is_compiled(void) const
{
    return m_compiled;
}

bool Graph::compile(const CompileOptions& options, std::string& error)
{
    error.clear();

    if(m_compiled && !m_dirty && m_reuse_targets == options.reuse_targets)
        return true;

    std::vector<Node*> roots;

    if(m_display) {
        auto node = m_display->as_node();

        if(!node) {
            error = "display source must be a shader node";
            return false;
        }

        roots.push_back(node);
    }

    if(m_output) {
        auto node = m_output->as_node();

        if(!node) {
            error = "output source must be a shader node";
            return false;
        }

        if(roots.empty() || roots.back() != node)
            roots.push_back(node);
    }

    if(roots.empty())
        LOG_WARNING("graph: no display or output source; nothing will be rendered");

    CompileState state;

    for(auto root : roots) {
        if(!visit_node(root, state)) {
            error = state.error;
            return false;
        }
    }

    std::unordered_map<Node*, std::size_t> last_use;

    for(auto node : state.plan)
        last_use[node] = 0;

    for(std::size_t index = 0; index < state.plan.size(); ++index) {
        auto node = state.plan[index];

        for(int channel = 0; channel < Node::MAX_CHANNELS; ++channel) {
            auto input = node->get_input(channel);

            if(!input)
                continue;

            auto dependency = input->as_node();

            if(!dependency)
                continue;

            auto found = last_use.find(dependency);

            if(found != last_use.end())
                found->second = std::max(found->second, index);
        }
    }

    for(auto root : roots)
        last_use[root] = std::numeric_limits<std::size_t>::max();

    std::vector<std::unique_ptr<RenderTarget>> new_targets;
    std::map<TargetKey, std::vector<RenderTarget*>> free_targets;
    std::unordered_map<Node*, RenderTarget*> assigned;
    std::unordered_map<Node*, RenderTarget*> node_targets;
    std::unordered_map<Node*, std::pair<RenderTarget*, RenderTarget*>> feedback_pairs;

    auto acquire = [&](const TargetKey& key) -> RenderTarget* {
        if(options.reuse_targets) {
            auto found = free_targets.find(key);

            if(found != free_targets.end() && !found->second.empty()) {
                auto target = found->second.back();
                found->second.pop_back();
                return target;
            }
        }

        new_targets.push_back(std::make_unique<RenderTarget>(key));
        return new_targets.back().get();
    };

    auto release = [&](Node* node) {
        auto found = assigned.find(node);

        if(found == assigned.end() || !found->second)
            return;

        auto target = found->second;
        found->second = nullptr;

        if(options.reuse_targets)
            free_targets[node->get_target_key()].push_back(target);
    };

    for(std::size_t index = 0; index < state.plan.size(); ++index) {
        auto node = state.plan[index];

        if(node->is_feedback()) {
            auto& key = node->get_target_key();
            new_targets.push_back(std::make_unique<RenderTarget>(key));
            auto front = new_targets.back().get();
            new_targets.push_back(std::make_unique<RenderTarget>(key));
            auto back = new_targets.back().get();
            front->clear();
            back->clear();
            feedback_pairs.emplace(node, std::make_pair(front, back));
        }
        else {
            auto target = acquire(node->get_target_key());
            assigned[node] = target;
            node_targets[node] = target;
        }

        for(int channel = 0; channel < Node::MAX_CHANNELS; ++channel) {
            auto input = node->get_input(channel);

            if(!input)
                continue;

            auto dependency = input->as_node();

            if(!dependency)
                continue;

            auto found = last_use.find(dependency);

            if(found != last_use.end() && found->second == index)
                release(dependency);
        }
    }

    for(auto& node : m_nodes)
        node->clear_targets();

    for(const auto& [node, target] : node_targets)
        node->set_target(target);

    for(const auto& [node, pair] : feedback_pairs)
        node->set_target_pair(pair.first, pair.second);

    m_targets = std::move(new_targets);
    m_plan = std::move(state.plan);
    m_compiled = true;
    m_dirty = false;
    m_reuse_targets = options.reuse_targets;

    return true;
}

void Graph::render(const Timings& timings)
{
    if(!m_compiled)
        return;

    Program::beginFrame();

    for(auto node : m_plan) {
        if(node->is_feedback())
            node->swap_targets();
    }

    for(auto node : m_plan)
        node->render(timings);
}

std::string Graph::dump_plan(void) const
{
    std::string result = std::format("plan: {} steps, {} targets\n", m_plan.size(), m_targets.size());

    std::map<TargetKey, std::pair<std::size_t, std::size_t>> buckets;
    std::size_t total_bytes = 0;

    for(const auto& target : m_targets) {
        auto& bucket = buckets[target->get_key()];
        bucket.first += 1;
        bucket.second += target->get_byte_size();
        total_bytes += target->get_byte_size();
    }

    for(const auto& [key, bucket] : buckets) {
        result += std::format("  bucket {}x{} {}: {} targets, {} bytes\n", key.width, key.height, format_name(key.format), bucket.first,
            bucket.second);
    }

    result += std::format("  total: {} bytes\n", total_bytes);

    std::unordered_map<const RenderTarget*, std::size_t> indices;

    for(std::size_t index = 0; index < m_targets.size(); ++index)
        indices.emplace(m_targets[index].get(), index);

    for(std::size_t index = 0; index < m_plan.size(); ++index) {
        auto node = m_plan[index];
        auto target = node->get_current_target();
        std::string target_name = "none";

        if(target) {
            auto found = indices.find(target);

            if(found != indices.end())
                target_name = std::format("T{}", found->second);
        }

        result += std::format("  #{} \"{}\" {}x{} -> {}{}\n", index, node->get_name(), node->get_texture_width(),
            node->get_texture_height(), target_name, node->is_feedback() ? " [feedback]" : "");
    }

    return result;
}
