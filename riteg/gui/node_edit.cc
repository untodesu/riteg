// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/core/globals.hh"
#include "riteg/core/logging.hh"
#include "riteg/graph/base_node.hh"
#include "riteg/graph/dest_display.hh"
#include "riteg/graph/dest_image.hh"
#include "riteg/graph/glsl_shader.hh"
#include "riteg/graph/shadertoy.hh"
#include "riteg/graph/src_blank.hh"
#include "riteg/graph/src_image.hh"
#include "riteg/gui/node_edit.hh"
#include "riteg/project/project.hh"

// All nodes share exact same slot kind
constexpr static int DEFAULT_SLOT_KIND = 42;

// Popup ID used to open/layout the popup frame
constexpr static const char *POPUP_ID = "NodeEdit_ContextMenu";

static ImNodes::Ez::SlotInfo output_slot = {};
static std::vector<ImNodes::Ez::SlotInfo> input_slots = {};
static std::vector<std::string> input_slot_names = {};
static bool has_selected_nodes = false;
static BaseNode *target_node = nullptr;

static void update_input_slots(BaseNode *node)
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

static void layout_dest_image(DestImageNode *node)
{
    ImGui::InputText("Name", &node->name);
    ImGui::NewLine();

    if(node->inputs[0] && node->inputs[0]->texture) {
        const float size = ImGui::CalcItemWidth();
        ImTextureID texture = reinterpret_cast<ImTextureID>(node->inputs[0]->texture);
        ImGui::Image(texture, ImVec2(size, size));
        ImGui::NewLine();
    }
}

static void layout_dest_display(DestDisplayNode *node)
{
    ImGui::InputText("Name", &node->name);
    ImGui::Checkbox("Always render", &node->always_render);
    ImGui::NewLine();

    if(node->inputs[0] && node->inputs[0]->texture) {
        const float size = ImGui::CalcItemWidth();
        ImTextureID texture = reinterpret_cast<ImTextureID>(node->inputs[0]->texture);
        ImGui::Image(texture, ImVec2(size, size));
        ImGui::NewLine();
    }
}

static void layout_src_blank(SrcBlankNode *node)
{
    ImGui::InputText("Name", &node->name);
    ImGui::NewLine();
    ImGui::ColorPicker4("Color", &node->color.x);
    ImGui::NewLine();
}

static void layout_src_image(SrcImageNode *node)
{
    ImGui::InputText("Name", &node->name);
    ImGui::Checkbox("Enable display", &node->enable_display);
    ImGui::NewLine();

    if(node->texture && node->enable_display) {
        const float size = ImGui::CalcItemWidth();
        ImTextureID texture = reinterpret_cast<ImTextureID>(node->texture);
        ImGui::Image(texture, ImVec2(size, size));
        ImGui::NewLine();
    }
}

static void layout_glsl_shader(GLSLShaderNode *node)
{
    ImGui::InputText("Name", &node->name);
    ImGui::NewLine();

    int inputs_count = node->inputs.size();

    if(ImGui::InputInt("Inputs count", &inputs_count)) {
        for(int i = inputs_count; i < node->inputs.size(); ++i) {
            if(node->inputs[i] != nullptr) {
                node->inputs[i]->outputs.erase(node);
                node->inputs[i] = nullptr;
            }
        }

        if(inputs_count <= 0)
            node->inputs.clear();
        else node->inputs.resize(inputs_count, nullptr);
        node->update_uniforms();
    }

    int texture_size[2] = {};
    texture_size[0] = node->texture_width;
    texture_size[1] = node->texture_height;

    if(!node->texture_width || !node->texture_height || ImGui::DragInt2("Output size", texture_size, 1.0f, 1, 4096)) {
        node->texture_width = texture_size[0] ? texture_size[0] : 1;
        node->texture_height = texture_size[1] ? texture_size[1] : 1;
        node->update_texture();
    }

    if(ImGui::InputText("Shader", &node->shader_path)) {
        node->update_shader();
        node->update_uniforms();
    }
    
    if(!node->shader_info_log.empty() || !node->program_info_log.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("[!!!]");
        if(ImGui::BeginItemTooltip()) {
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            if(!node->shader_info_log.empty())
                ImGui::TextUnformatted(node->shader_info_log.c_str());
            if(!node->program_info_log.empty())
                ImGui::TextUnformatted(node->program_info_log.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    if(ImGui::Button("Force recompile", ImVec2(ImGui::CalcItemWidth(), 0.0f))) {
        node->update_shader();
        node->update_uniforms();
    }

    ImGui::NewLine();

    int params_count = node->params.size();

    if(ImGui::InputInt("Params count", &params_count)) {
        if(params_count >= 1)
            node->params.resize(params_count, 0.0f);
        else node->params.clear();
    }

    char tmp_buffer[256] = {};
    for(std::size_t i = 0; i < node->params.size(); ++i) {
        std::snprintf(tmp_buffer, sizeof(tmp_buffer), "u_Params[%zu]", i);
        ImGui::DragFloat(tmp_buffer, &node->params[i], 1.0f, 0.0f, 100.0f);
    }
}

static void layout_shadertoy(ShadertoyNode *node)
{
    ImGui::InputText("Name", &node->name);
    ImGui::NewLine();

    int texture_size[2] = {};
    texture_size[0] = node->texture_width;
    texture_size[1] = node->texture_height;

    if(!node->texture_width || !node->texture_height || ImGui::DragInt2("Output size", texture_size, 1.0f, 1, 4096)) {
        node->texture_width = texture_size[0] ? texture_size[0] : 1;
        node->texture_height = texture_size[1] ? texture_size[1] : 1;
        node->update_texture();
    }

    if(ImGui::InputText("Shader", &node->shader_path)) {
        node->update_shader();
        node->update_uniforms();
    }
    
    if(!node->shader_info_log.empty() || !node->program_info_log.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("[!!!]");
        if(ImGui::BeginItemTooltip()) {
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            if(!node->shader_info_log.empty())
                ImGui::TextUnformatted(node->shader_info_log.c_str());
            if(!node->program_info_log.empty())
                ImGui::TextUnformatted(node->program_info_log.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    if(ImGui::Button("Force recompile", ImVec2(ImGui::CalcItemWidth(), 0.0f))) {
        node->update_shader();
        node->update_uniforms();
    }

    ImGui::NewLine();
}

static void layout_popup_add(void)
{
    if(ImGui::MenuItem("Source - image")) {
        SrcImageNode *node = new SrcImageNode();
        node->name = "Image source";
        node->id = project::random_dev();
        ImNodes::AutoPositionNode(node);
        project::tree.insert(node);
    }

    if(ImGui::MenuItem("Source - blank")) {
        SrcBlankNode *node = new SrcBlankNode();
        node->name = "Blank source";
        node->id = project::random_dev();
        ImNodes::AutoPositionNode(node);
        project::tree.insert(node);
    }

    if(ImGui::MenuItem("Source - feedback")) {
        // TODO: add feedback source
    }

    ImGui::Separator();

    if(ImGui::MenuItem("Target - display")) {
        DestDisplayNode *node = new DestDisplayNode();
        node->name = "Display target";
        node->id = project::random_dev();
        ImNodes::AutoPositionNode(node);
        project::tree.insert(node);
    }

    ImGui::BeginDisabled(project::dest_image != nullptr);

    if(ImGui::MenuItem("Target - image") && !project::dest_image) {
        DestImageNode *node = new DestImageNode();
        node->name = "Image target";
        node->id = static_cast<unsigned long>(0);
        ImNodes::AutoPositionNode(node);
        project::tree.insert(node);
        project::dest_image = node;
    }

    ImGui::EndDisabled();

    ImGui::Separator();

    if(ImGui::MenuItem("Shader - GLSL")) {
        GLSLShaderNode *node = new GLSLShaderNode();
        node->name = "GLSL shader";
        node->id = project::random_dev();
        ImNodes::AutoPositionNode(node);
        project::tree.insert(node);
    }

    if(ImGui::MenuItem("Shader - Shadertoy")) {
        ShadertoyNode *node = new ShadertoyNode();
        node->name = "Shadertoy shader";
        node->id = project::random_dev();
        ImNodes::AutoPositionNode(node);
        project::tree.insert(node);
    }
}

static void layout_popup_node_ops(void)
{
    if(ImGui::MenuItem("Remove")) {
        if(target_node->get_type() == NODE_DEST_IMAGE)
            project::dest_image = nullptr;
        project::tree.erase(target_node);
        globals::render_list.erase(target_node);
        delete target_node;
    }

    if(ImGui::MenuItem("Render")) {
        globals::render_list.insert(target_node);
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

    if(!ImGui::Begin("Graph Editor###NodeEdit_Window")) {
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
    
    for(BaseNode *node : project::tree) {
        update_input_slots(node);

        if(ImNodes::Ez::BeginNode(node, node->name.c_str(), &node->position, &node->selected)) {
            switch(node->get_type()) {
                case NODE_DEST_IMAGE:
                    ImNodes::Ez::InputSlots(input_slots.data(), 1);
                    layout_dest_image(static_cast<DestImageNode *>(node));
                    ImNodes::Ez::OutputSlots(nullptr, 0);
                    break;
                case NODE_DEST_DISPLAY:
                    ImNodes::Ez::InputSlots(input_slots.data(), 1);
                    layout_dest_display(static_cast<DestDisplayNode *>(node));
                    ImNodes::Ez::OutputSlots(nullptr, 0);
                    break;
                case NODE_SRC_BLANK:
                    ImNodes::Ez::InputSlots(nullptr, 0);
                    layout_src_blank(static_cast<SrcBlankNode *>(node));
                    ImNodes::Ez::OutputSlots(&output_slot, 1);
                    break;
                case NODE_SRC_IMAGE:
                    ImNodes::Ez::InputSlots(nullptr, 0);
                    layout_src_image(static_cast<SrcImageNode *>(node));
                    ImNodes::Ez::OutputSlots(&output_slot, 1);
                    break;
                case NODE_GLSL_SHADER:
                    ImNodes::Ez::InputSlots(input_slots.data(), node->inputs.size());
                    layout_glsl_shader(static_cast<GLSLShaderNode *>(node));
                    ImNodes::Ez::OutputSlots(&output_slot, 1);
                    break;
                case NODE_SHADERTOY:
                    ImNodes::Ez::InputSlots(input_slots.data(), node->inputs.size());
                    layout_shadertoy(static_cast<ShadertoyNode *>(node));
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
        BaseNode *src_node = reinterpret_cast<BaseNode *>(src_ptr);
        BaseNode *dst_node = reinterpret_cast<BaseNode *>(dst_ptr);
        std::size_t dst_index = SIZE_MAX;

        // ImNodes seems to give us the exact same pointers as
        // the strings in the input_slots[i].title; we can use this
        // and avoid both string comparison and/or std::sscanf usage
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
