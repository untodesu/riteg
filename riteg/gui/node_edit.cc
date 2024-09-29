// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/core/globals.hh"
#include "riteg/core/logging.hh"
#include "riteg/graph/node.hh"
#include "riteg/graph/shader_pass.hh"
#include "riteg/graph/target_imgui.hh"
#include "riteg/gui/node_edit.hh"

constexpr static int DEFAULT_SLOT_KIND = 1;

static ImNodes::Ez::SlotInfo output_slot = {};
static std::vector<ImNodes::Ez::SlotInfo> input_slots = {};
static std::vector<std::string> input_slot_names = {};

static void layout_target_imgui(TargetImGuiNode *node)
{
    ImNodes::Ez::InputSlots(input_slots.data(), 1);
    ImNodes::Ez::OutputSlots(nullptr, 0);
}

static void layout_shader_pass(ShaderPassNode *node)
{
    ImNodes::Ez::InputSlots(input_slots.data(), static_cast<int>(node->inputs.size()));

    bool make_texture = false;
    make_texture = make_texture || ImGui::InputInt("Texture width", &node->texture_width);
    make_texture = make_texture || ImGui::InputInt("Texture height", &node->texture_height);

    if(make_texture) {
        if(node->texture_width < 1) node->texture_width = 1;
        if(node->texture_height < 1) node->texture_height = 1;

        if(!node->texture)
            glGenTextures(1, &node->texture);
        glBindTexture(GL_TEXTURE_2D, node->texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, node->texture_width, node->texture_height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

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

    ImGui::PushItemWidth(1.60f * ImGui::CalcItemWidth());

    char buffer[256] = {};
    for(std::size_t i = 0; i < node->params.size(); ++i) {
        std::snprintf(buffer, sizeof(buffer), "[%zu]", i);
        ImGui::InputFloat(buffer, &node->params[i]);
    }

    ImGui::PopItemWidth();
    ImNodes::Ez::OutputSlots(&output_slot, 1);
}

void node_edit::init(void)
{
    output_slot.title = "Output";
    output_slot.kind = DEFAULT_SLOT_KIND;


    //
    // FIXME: Move this somewhere else ASAP
    //

    char staging_buffer[256] = {};
    for(std::size_t i = 0; i < 2; ++i) {
        std::snprintf(staging_buffer, sizeof(staging_buffer), "Node[%zu]", i);
        ShaderPassNode *node = new ShaderPassNode(staging_buffer, globals::random_dev());
        node->inputs.resize(2, nullptr);
        globals::pr_nodes.insert(node);
    }

    TargetImGuiNode *node = new TargetImGuiNode("Display", globals::random_dev());
    globals::pr_nodes.insert(node);
}

void node_edit::layout(void)
{
    char buffer[256] = {};

    if(!ImGui::Begin("Node Editor###NodeEdit_Window")) {
        ImGui::End();
        return;
    }

    ImNodes::Ez::BeginCanvas();

    const float item_width = 0.125f * ImGui::CalcItemWidth();
    const float zoom_factor = ImNodes::GetCurrentCanvas()->Zoom;
    ImGui::PushItemWidth(item_width * zoom_factor);

    for(Node *node : globals::pr_nodes) {
        if(input_slots.size() < node->inputs.size()) {
            // Dynamically grow the string array until
            // we have enough; as per wasting allocations
            // C++ documentation states vectors grow in chunks
            while(input_slot_names.size() < node->inputs.size()) {
                std::snprintf(buffer, sizeof(buffer), "Input[%zu]", input_slot_names.size());
                input_slot_names.push_back(std::string(buffer));
            }

            input_slots.resize(node->inputs.size(), ImNodes::Ez::SlotInfo());

            for(std::size_t i = 0; i < input_slots.size(); ++i) {
                input_slots[i].title = input_slot_names[i].c_str();
                input_slots[i].kind = DEFAULT_SLOT_KIND;
            }
        }

        if(ImNodes::Ez::BeginNode(node, node->title.c_str(), &node->position, &node->selected)) {            
            switch(node->get_type()) {
                case Node::SOURCE_IMAGE:
                    // layout_source_image(static_cast<SourceImageNode *>(node));
                    break;
                case Node::SOURCE_EMPTY:
                    // layout_source_empty(static_cast<SourceEmptyNode *>(node));
                    break;
                case Node::SOURCE_NOISE:
                    // layout_source_noise(static_cast<SourceNoiseNode *>(node));
                    break;
                case Node::TARGET_IMAGE:
                    // layout_target_image(static_cast<TargetImageNode *>(node));
                    break;
                case Node::TARGET_IMGUI:
                    layout_target_imgui(static_cast<TargetImGuiNode *>(node));
                    break;
                case Node::SHADER_PASS:
                    layout_shader_pass(static_cast<ShaderPassNode *>(node));
                    break;
                default:
                    ImGui::TextUnformatted("If you see this, the code is fucked");
                    break;
            }            

            ImNodes::Ez::EndNode();
        }

        for(std::size_t i = 0; i < node->inputs.size(); ++i) {
            if(node->inputs[i] != nullptr) {
                const char *src_cstr = output_slot.title;
                const char *dst_cstr = input_slots[i].title;

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
        std::size_t dst_index = SIZE_MAX;

        // ImNodes seems to give us the exact same pointers as
        // the strings in the input_slots[i].title; we can use this
        // and avoid both string comparison and std::sscanf usage
        for(std::size_t i = 0; i < dst_node->inputs.size(); ++i) {
            if(input_slots[i].title == dst_cstr) {
                dst_index = i;
                break;
            }
        }

        if(dst_index < dst_node->inputs.size()) {
            dst_node->inputs[dst_index] = src_node;
            src_node->outputs.insert(dst_node);
        }
    }

    ImGui::PopItemWidth();
    ImNodes::Ez::EndCanvas();
    ImGui::End();
}
