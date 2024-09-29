// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/core/globals.hh"
#include "riteg/core/logging.hh"
#include "riteg/graph/node.hh"
#include "riteg/graph/shader_pass.hh"
#include "riteg/graph/target_imgui.hh"
#include "riteg/gui/node_edit.hh"

// All nodes share exact same slot kind
constexpr static int DEFAULT_SLOT_KIND = 42;

// Popup ID used to open/layout the popup frame
constexpr static const char *POPUP_ID = "NodeEdit_ContextMenu";

static ImNodes::Ez::SlotInfo output_slot = {};
static std::vector<ImNodes::Ez::SlotInfo> input_slots = {};
static std::vector<std::string> input_slot_names = {};
static bool has_selected_nodes = false;
static Node *target_node = nullptr;

static void update_input_slots(Node *node)
{
    if(input_slots.size() < node->inputs.size()) {
        // Dynamically grow the string array until
        // we have enough; as per wasting allocations
        // C++ documentation states vectors grow in chunks
        while(input_slot_names.size() < node->inputs.size()) {
            char staging_buffer[256] = {};
            std::snprintf(staging_buffer, sizeof(staging_buffer), "Input[%zu]", input_slot_names.size());
            input_slot_names.push_back(std::string(staging_buffer));
        }

        input_slots.resize(node->inputs.size(), ImNodes::Ez::SlotInfo());

        for(std::size_t i = 0; i < input_slots.size(); ++i) {
            input_slots[i].title = input_slot_names[i].c_str();
            input_slots[i].kind = DEFAULT_SLOT_KIND;
        }
    }
}

static void layout_target_imgui(TargetImGuiNode *node)
{
    ImGui::InputText("Name", &node->title);
    ImGui::NewLine();
}

static void layout_shader_pass(ShaderPassNode *node)
{
    int inputs_count_i = node->inputs.size();
    int texture_width_i = node->texture_width;
    int texture_height_i = node->texture_height;
    int u_node_params_count_i = node->params.size();

    ImGui::InputText("Title", &node->title);
    ImGui::InputInt("Inputs count", &inputs_count_i);
    ImGui::InputInt("Output width", &texture_width_i);
    ImGui::InputInt("Output height", &texture_height_i);
    ImGui::NewLine();

    if(inputs_count_i <= 0) inputs_count_i = 0;
    if(texture_width_i <= 0) texture_width_i = 1;
    if(texture_height_i <= 0) texture_height_i = 1;

    if(inputs_count_i != node->inputs.size()) {
        for(int i = inputs_count_i; i < node->inputs.size(); ++i) {
            if(node->inputs[i] != nullptr) {
                node->inputs[i]->outputs.erase(node);
                node->inputs[i] = nullptr;
            }
        }

        node->inputs.resize(inputs_count_i, nullptr);
    }

    if((texture_width_i != node->texture_width) || (texture_height_i != node->texture_height)) {
        if(!node->texture)
            glGenTextures(1, &node->texture);
        glBindTexture(GL_TEXTURE_2D, node->texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texture_width_i, texture_height_i, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        node->texture_height = texture_height_i;
        node->texture_width = texture_width_i;
    }

    if(ImGui::InputInt("Params count", &u_node_params_count_i)) {
        if(u_node_params_count_i >= 1)
            node->params.resize(u_node_params_count_i, 0.0f);
        else node->params.clear();
    }

    char tmp_buffer[256] = {};
    for(std::size_t i = 0; i < node->params.size(); ++i) {
        std::snprintf(tmp_buffer, sizeof(tmp_buffer), "u_Params[%zu]", i);
        ImGui::InputFloat(tmp_buffer, &node->params[i]);
    }
}

static void layout_popup_add(void)
{
    if(ImGui::MenuItem("Image source")) {
        // TODO: add image source
    }

    if(ImGui::MenuItem("Blank source")) {
        // TODO: add blank source
    }

    if(ImGui::MenuItem("Feedback source")) {
        // TODO: add feedback source
    }

    ImGui::Separator();

    if(ImGui::MenuItem("Image target")) {
        // TODO: add image target
    }

    if(ImGui::MenuItem("Display target")) {
        TargetImGuiNode *node = new TargetImGuiNode("Display", globals::random_dev());
        ImNodes::AutoPositionNode(node);
        globals::pr_nodes.insert(node);
    }

    ImGui::Separator();

    if(ImGui::MenuItem("Shader pass")) {
        ShaderPassNode *node = new ShaderPassNode("Shader pass", globals::random_dev());
        ImNodes::AutoPositionNode(node);
        globals::pr_nodes.insert(node);
    }
}

static void layout_popup_node_ops(void)
{
    if(ImGui::MenuItem("Remove")) {
        globals::pr_nodes.erase(target_node);
        delete target_node;
    }

    if(ImGui::MenuItem("Render")) {
        target_node->render();
    }
}

void node_edit::init(void)
{
    output_slot.title = "Output";
    output_slot.kind = DEFAULT_SLOT_KIND;
    target_node = nullptr;
}

void node_edit::layout(void)
{
    char buffer[256] = {};


    if(!ImGui::Begin("Node Editor###NodeEdit_Window")) {
        ImGui::End();
        return;
    }

    ImNodes::Ez::BeginCanvas();

    const float item_width = 0.25f * ImGui::CalcItemWidth();
    const float zoom_factor = ImNodes::GetCurrentCanvas()->Zoom;
    ImGui::PushItemWidth(item_width * zoom_factor);

    // This is set to true whenever we want to open a context menu this frame;
    // it is kept as a separate boolean because it's also checked multiple times across the layout
    const bool open_context_menu = ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::IsWindowHovered();

    has_selected_nodes = false;

    if(open_context_menu) {
        ImGui::OpenPopup(POPUP_ID);
        target_node = nullptr;
    }
    
    for(Node *node : globals::pr_nodes) {
        update_input_slots(node);

        if(ImNodes::Ez::BeginNode(node, node->title.c_str(), &node->position, &node->selected)) {
            switch(node->get_type()) {
                case Node::TARGET_IMGUI:
                    ImNodes::Ez::InputSlots(input_slots.data(), 1);
                    layout_target_imgui(static_cast<TargetImGuiNode *>(node));
                    ImNodes::Ez::OutputSlots(nullptr, 0);
                    break;
                case Node::SHADER_PASS:
                    ImNodes::Ez::InputSlots(input_slots.data(), node->inputs.size());
                    layout_shader_pass(static_cast<ShaderPassNode *>(node));
                    ImNodes::Ez::OutputSlots(&output_slot, 1);
                    break;
                default:
                    ImNodes::Ez::InputSlots(nullptr, 0);
                    ImGui::TextUnformatted("If you see this, the code is fucked");
                    ImNodes::Ez::OutputSlots(nullptr, 0);
                    break;
            }

            if(node->selected) has_selected_nodes = true;
            if(open_context_menu && ImNodes::IsNodeHovered()) target_node = node;
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

    if(ImGui::BeginPopup(POPUP_ID, ImGuiWindowFlags_None)) {
        if(ImGui::BeginMenu("Add")) {
            layout_popup_add();
            ImGui::EndMenu();
        }

        ImGui::Separator();

        ImGui::BeginDisabled(!target_node);
        layout_popup_node_ops();
        ImGui::EndDisabled();

        ImGui::EndPopup();
    }

    ImNodes::Ez::EndCanvas();
    ImGui::End();
}
