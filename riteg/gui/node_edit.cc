// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/core/logging.hh"
#include "riteg/graph/base_node.hh"
#include "riteg/graph/shader_node.hh"
#include "riteg/gui/node_edit.hh"

constexpr static int TEX_SLOT_KIND = 1;

// FIXME: move this somewhere else ASAP
static std::vector<BaseNode *> s_nodes = {};

static ImNodes::Ez::SlotInfo basic_input = {};
static ImNodes::Ez::SlotInfo basic_output = {};
static std::vector<ImNodes::Ez::SlotInfo> tex_slots = {};
static std::vector<std::string> tex_slot_names = {};

static void layout_shader_node(ShaderNode *node)
{
    const std::size_t input_count = node->inputs.size();

    if(tex_slots.size() < input_count) {
        // Dynamically grow the string array until
        // we have enough; as per wasting allocations
        // C++ documentation states vectors grow in chunks
        while(tex_slot_names.size() < input_count) {
            char staging_buffer[256] = {};
            std::snprintf(staging_buffer, sizeof(staging_buffer), "[%zu]", tex_slot_names.size());
            tex_slot_names.push_back(std::string(staging_buffer));
        }

        tex_slots.resize(input_count);

        for(std::size_t i = 0; i < input_count; ++i) {
            tex_slots[i].title = tex_slot_names[i].c_str();
            tex_slots[i].kind = TEX_SLOT_KIND;
        }
    }

    ImNodes::Ez::InputSlots(tex_slots.data(), static_cast<int>(input_count));
    ImNodes::Ez::OutputSlots(&basic_output, 1);
}

void node_edit::init_remove_me_asap(void)
{
    basic_input.title = "Input";
    basic_input.kind = TEX_SLOT_KIND;

    basic_output.title = "Output";
    basic_output.kind = TEX_SLOT_KIND;

    // Create a basic shader node
    ShaderNode *nx = new ShaderNode();
    nx->inputs.resize(2, nullptr);
    nx->title = std::string("Test Node [1]");
    nx->position = ImVec2(50.0f, 100.0f);
    nx->selected = false;

    // Create a basic shader node
    ShaderNode *ny = new ShaderNode();
    ny->inputs.resize(4, nullptr);
    ny->title = std::string("Test Node [2]");
    ny->position = ImVec2(250.0f, 100.0f);
    ny->selected = false;

    ny->inputs[0] = nx;
    ny->inputs[2] = nx;

    s_nodes.push_back(nx);
    s_nodes.push_back(ny);
}

void node_edit::layout(void)
{
    if(!ImGui::Begin("NodeEdit###NodeEdit_Window")) {
        ImGui::End();
        return;
    }

    ImNodes::Ez::BeginCanvas();

    for(BaseNode *node : s_nodes) {
        const NodeType type = node->get_type();

        if(ImNodes::Ez::BeginNode(node, node->title.c_str(), &node->position, &node->selected)) {
            switch(type) {
            case NodeType::SOURCE_NODE:
                // layout_source_node(static_cast<SourceNode *>(node));
                break;
            case NodeType::TARGET_NODE:
                // layout_target_node(static_cast<TargetNode *>(node));
                break;
            case NodeType::SHADER_NODE:
                layout_shader_node(static_cast<ShaderNode *>(node));
                break;
            default:
                ImGui::TextUnformatted("If you see this, the code is fucked");
                break;
            }

            ImNodes::Ez::EndNode();
        }

        const std::size_t input_count = node->inputs.size();
        for(std::size_t i = 0; i < input_count; ++i) {
            if(node->inputs[i] != nullptr) {
                const char *input_title = tex_slots[i].title;
                const char *output_title = basic_output.title;

                if(!ImNodes::Connection(node, input_title, node->inputs[i], output_title)) {
                    node->inputs[i]->outputs.erase(node);
                    node->inputs[i] = nullptr;
                    continue;
                }
            }
        }
    }

    void *input_ptr = nullptr;
    void *output_ptr = nullptr;
    const char *input_slot = nullptr;
    const char *output_slot = nullptr;
    if(ImNodes::GetNewConnection(&input_ptr, &input_slot, &output_ptr, &output_slot)) {
        BaseNode *input_node = reinterpret_cast<BaseNode *>(input_ptr);
        BaseNode *output_node = reinterpret_cast<BaseNode *>(output_ptr);

        std::size_t input_index;
        std::sscanf(input_slot, "[%zu]", &input_index);

        if(input_node->inputs.size() > input_index) {
            input_node->inputs[input_index] = output_node;
            output_node->outputs.insert(input_node);
        }
    }

    ImNodes::Ez::EndCanvas();
    ImGui::End();
}
