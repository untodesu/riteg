// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/core/globals.hh"
#include "riteg/core/logging.hh"
#include "riteg/graph/node.hh"
#include "riteg/graph/shader.hh"
#include "riteg/gui/node_edit.hh"

constexpr static int DEFAULT_SLOT_KIND = 1;

static ImNodes::Ez::SlotInfo basic_input = {};
static ImNodes::Ez::SlotInfo basic_output = {};
static std::vector<ImNodes::Ez::SlotInfo> tex_slots = {};
static std::vector<std::string> tex_slot_names = {};

static void layout_shader_node(ShaderNode *node)
{
    char buffer[256] = {0};

    if(tex_slots.size() < node->inputs.size()) {
        // Dynamically grow the string array until
        // we have enough; as per wasting allocations
        // C++ documentation states vectors grow in chunks
        while(tex_slot_names.size() < node->inputs.size()) {
            std::snprintf(buffer, sizeof(buffer), "iTexture%zu", tex_slot_names.size());
            tex_slot_names.push_back(std::string(buffer));
        }

        tex_slots.resize(node->inputs.size(), ImNodes::Ez::SlotInfo());

        for(std::size_t i = 0; i < tex_slots.size(); ++i) {
            tex_slots[i].title = tex_slot_names[i].c_str();
            tex_slots[i].kind = DEFAULT_SLOT_KIND;
        }
    }

    ImNodes::Ez::InputSlots(tex_slots.data(), static_cast<int>(tex_slots.size()));

    ImGui::BeginDisabled();
    std::uint64_t num_params = static_cast<std::uint64_t>(node->params.size());
    ImGui::InputScalar("", ImGuiDataType_U64, &num_params);
    ImGui::EndDisabled();
    ImGui::SameLine();

    // Decrease parameter count
    if(ImGui::Button(" - ") && node->params.size())
        node->params.pop_back();
    ImGui::SameLine();

    // Increase parameter count
    if(ImGui::Button(" + "))
        node->params.push_back(0.0f);
    ImGui::NewLine();

    ImGui::PushItemWidth(1.5f * ImGui::CalcItemWidth());

    for(std::size_t i = 0; i < node->params.size(); ++i) {
        std::snprintf(buffer, sizeof(buffer), "[%zu]", i);
        ImGui::InputFloat(buffer, &node->params[i]);
    }

    ImGui::PopItemWidth();
    ImNodes::Ez::OutputSlots(&basic_output, 1);
}

void node_edit::init(void)
{
    basic_input.title = "Input";
    basic_input.kind = DEFAULT_SLOT_KIND;

    basic_output.title = "Output";
    basic_output.kind = DEFAULT_SLOT_KIND;


    //
    // FIXME: Move this somewhere else ASAP
    //

    char staging_buffer[256] = {};
    for(std::size_t i = 0; i < 4; ++i) {
        std::snprintf(staging_buffer, sizeof(staging_buffer), "Node[%zu]", i);
        ShaderNode *node = new ShaderNode(staging_buffer);
        node->inputs.resize(4, nullptr);
        globals::pr_nodes.insert(node);
    }
}

void node_edit::layout(void)
{
    if(!ImGui::Begin("NodeEdit###NodeEdit_Window")) {
        ImGui::End();
        return;
    }

    ImNodes::Ez::BeginCanvas();

    const float item_width = 0.125f * ImGui::CalcItemWidth();
    const float zoom_factor = ImNodes::GetCurrentCanvas()->Zoom;
    ImGui::PushItemWidth(item_width * zoom_factor);

    for(Node *node : globals::pr_nodes) {
        if(ImNodes::Ez::BeginNode(node, node->title.c_str(), &node->position, &node->selected)) {
            switch(node->get_type()) {
                case Node::SOURCE: break;
                case Node::TARGET: break;
                case Node::SHADER: layout_shader_node(static_cast<ShaderNode *>(node)); break;
            }            

            ImNodes::Ez::EndNode();
        }

        for(std::size_t i = 0; i < node->inputs.size(); ++i) {
            if(node->inputs[i] != nullptr) {
                const char *src_cstr = basic_output.title;
                const char *dst_cstr = tex_slots[i].title;

                if(!ImNodes::Connection(node, dst_cstr, node->inputs[i], src_cstr)) {
                    node->inputs[i]->outputs.erase(node);
                    node->inputs[i] = nullptr;
                }
            }
        }
    }

    void *src_ptr = nullptr;
    void *dst_ptr = nullptr;
    const char *src_cstr = nullptr;
    const char *dst_cstr = nullptr;

    if(ImNodes::GetNewConnection(&dst_ptr, &dst_cstr, &src_ptr, &src_cstr)) {
        Node *src_node = reinterpret_cast<Node *>(src_ptr);
        Node *dst_node = reinterpret_cast<Node *>(dst_ptr);

        std::size_t dst_index = {};
        std::sscanf(dst_cstr, "iTexture%zu", &dst_index);

        if(dst_index < dst_node->inputs.size()) {
            dst_node->inputs[dst_index] = src_node;
            src_node->outputs.insert(dst_node);
        }
    }

    ImGui::PopItemWidth();
    ImNodes::Ez::EndCanvas();
    ImGui::End();
}
