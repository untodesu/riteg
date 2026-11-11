#pragma once

#include "riteg/render_target.hh"

class Node;
class Program;
class Source;
struct Timings;

struct NodeOptions final {
    std::string name;
    TextureFormat format = TextureFormat::RGBA16F;
    TextureFilter filter = TextureFilter::Linear;
    TextureWrap wrap = TextureWrap::Clamp;
};

struct CompileOptions final {
    bool reuse_targets = true;
};

class Graph final {
public:
    Graph(void);
    ~Graph(void);

    Graph(const Graph&) = delete;
    Graph& operator=(const Graph&) = delete;

    Program* get_program(const std::string& source, std::string& error);
    Node* create_node(Program* program, int width, int height, const NodeOptions& options);
    void connect(Node* node, int channel, Source* source);

    bool set_output(Source* source, std::string& error);
    bool set_display(Source* source, std::string& error);
    Source* get_output(void) const;
    Source* get_display(void) const;

    bool compile(const CompileOptions& options, std::string& error);
    bool is_compiled(void) const;
    void render(const Timings& timings);
    std::string dump_plan(void) const;
    void clear(void);

private:
    std::vector<std::unique_ptr<Program>> m_programs;
    std::unordered_map<std::string, Program*> m_program_cache;
    std::vector<std::unique_ptr<Node>> m_nodes;
    std::vector<std::unique_ptr<RenderTarget>> m_targets;
    std::vector<Node*> m_plan;

    Source* m_output = nullptr;
    Source* m_display = nullptr;

    bool m_compiled = false;
    bool m_dirty = true;
    bool m_reuse_targets = true;
};
