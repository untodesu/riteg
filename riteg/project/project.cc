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
#include "riteg/project/input.hh"
#include "riteg/project/project.hh"

std::filesystem::path project::directory = {};
std::filesystem::path project::json_path = {};
std::string project::imgui_ini_path = {};

std::string project::description = {};

std::random_device project::random_dev = {};
std::unordered_set<BaseNode *> project::tree = {};
DestImageNode *project::dest_image = nullptr;

static bool parse_json(void)
{
    JSON_Value *jsonv = json_parse_file(project::json_path.string().c_str());
    JSON_Object *json = json_value_get_object(jsonv);

    if(!jsonv) {
        logging::warn("project::open: can't open %s", project::json_path.generic_string().c_str());
        return false;
    }

    if(!json) {
        logging::warn("project::open: %s is not a valid JSON file", project::json_path.generic_string().c_str());
        json_value_free(jsonv);
        return false;
    }

    if(const char *description = json_object_get_string(json, "description"))
        project::description = std::string(description);
    else project::description = std::string();

    input::mode = INPUT_MODE_NULL;
    input::directory = std::filesystem::path();
    input::format = std::string();

    if(const char *input_mode = json_object_get_string(json, "input.mode")) {
        if(!std::string("disabled").compare(input_mode))
            input::mode = INPUT_MODE_NULL;
        else if(!std::string("directory_iterator").compare(input_mode))
            input::mode = INPUT_MODE_DIRECTORY_ITERATOR;
        else if(!std::string("std_sprintf").compare(input_mode))
            input::mode = INPUT_MODE_STD_SPRINTF;
        else logging::warn("project::open: non-valid input mode %s", input_mode);
    }

    if(const char *input_directory = json_object_get_string(json, "input.directory"))
        input::directory = std::filesystem::path(input_directory);
    else input::directory = std::filesystem::path();

    if(const char *input_format = json_object_get_string(json, "input.format"))
        input::format = std::string(input_format);
    else input::format = std::string();

    input::update_paths();
    input::reload_image();

    const JSON_Array *tree = json_object_get_array(json, "tree");
    const std::size_t tree_count = json_array_get_count(tree);

    for(std::size_t i = 0; i < tree_count; ++i) {
        const JSON_Object *node = json_array_get_object(tree, i);

        if(!node) {
            logging::warn("project::open: tree[%zu] is not a valid JSON object", i);
            continue;
        }

        const unsigned long id = json_object_get_number(node, "id");
        const char *type_str = json_object_get_string(node, "type");
        const char *name_str = json_object_get_string(node, "name");
        const JSON_Array *position = json_object_get_array(node, "position");
        const JSON_Array *texture_size = json_object_get_array(node, "texture_size");

        if(!type_str) {
            logging::warn("project::open: tree[%zu].type is not a valid JSON string", i);
            continue;
        }

        if(!name_str) {
            logging::warn("project::open: tree[%zu].name is not a valid JSON string", i);
            continue;
        }

        if(!std::string("dest_display").compare(type_str)) {
            DestDisplayNode *dest_display = new DestDisplayNode();
            dest_display->id = id;
            dest_display->name = std::string(name_str);
            dest_display->position.x = json_array_get_number(position, 0);
            dest_display->position.y = json_array_get_number(position, 1);
            dest_display->always_render = json_object_get_boolean(node, "always_render");
            project::tree.insert(dest_display);
            continue;
        }

        if(!std::string("dest_image").compare(type_str) && !project::dest_image) {
            DestImageNode *dest_image = new DestImageNode();
            dest_image->id = id;
            dest_image->name = std::string(name_str);
            dest_image->position.x = json_array_get_number(position, 0);
            dest_image->position.y = json_array_get_number(position, 1);
            project::dest_image = dest_image;
            project::tree.insert(dest_image);
            continue;
        }

        if(!std::string("glsl_shader").compare(type_str)) {
            GLSLShaderNode *glsl_shader = new GLSLShaderNode();
            glsl_shader->id = id;
            glsl_shader->name = std::string(name_str);
            glsl_shader->position.x = json_array_get_number(position, 0);
            glsl_shader->position.y = json_array_get_number(position, 1);
            if(const char *shader_str = json_object_get_string(node, "shader"))
                glsl_shader->shader_path = std::string(shader_str);
            glsl_shader->texture_width = json_array_get_number(texture_size, 0);
            glsl_shader->texture_height = json_array_get_number(texture_size, 1);

            if(const JSON_Array *params = json_object_get_array(node, "params")) {
                const std::size_t params_count = json_array_get_count(params);
                for(std::size_t j = 0; j < params_count; ++j) {
                    glsl_shader->params.push_back(json_array_get_number(params, j));
                }
            }

            glsl_shader->update_shader();
            glsl_shader->update_uniforms();
            glsl_shader->update_texture();

            project::tree.insert(glsl_shader);
            continue;
        }

        if(!std::string("shadertoy").compare(type_str)) {
            ShadertoyNode *shadertoy = new ShadertoyNode();
            shadertoy->id = id;
            shadertoy->name = std::string(name_str);
            shadertoy->position.x = json_array_get_number(position, 0);
            shadertoy->position.y = json_array_get_number(position, 1);
            if(const char *shader_str = json_object_get_string(node, "shader"))
                shadertoy->shader_path = std::string(shader_str);
            shadertoy->texture_width = json_array_get_number(texture_size, 0);
            shadertoy->texture_height = json_array_get_number(texture_size, 1);

            shadertoy->update_shader();
            shadertoy->update_uniforms();
            shadertoy->update_texture();

            project::tree.insert(shadertoy);
            continue;
        }

        if(!std::string("src_blank").compare(type_str)) {
            SrcBlankNode *src_blank = new SrcBlankNode();
            src_blank->id = id;
            src_blank->name = std::string(name_str);
            src_blank->position.x = json_array_get_number(position, 0);
            src_blank->position.y = json_array_get_number(position, 1);

            if(const JSON_Array *color = json_object_get_array(node, "color")) {
                src_blank->color.x = json_array_get_number(color, 0);
                src_blank->color.y = json_array_get_number(color, 1);
                src_blank->color.z = json_array_get_number(color, 2);
                src_blank->color.w = json_array_get_number(color, 3);
            }

            project::tree.insert(src_blank);
            continue;
        }

        if(!std::string("src_image").compare(type_str)) {
            SrcImageNode *src_image = new SrcImageNode();
            src_image->id = id;
            src_image->name = std::string(name_str);
            src_image->position.x = json_array_get_number(position, 0);
            src_image->position.y = json_array_get_number(position, 1);
            src_image->enable_display = json_object_get_boolean(node, "enable_display");
            project::tree.insert(src_image);
            continue;
        }
    }

    for(std::size_t i = 0; i < tree_count; ++i) {
        const JSON_Object *node = json_array_get_object(tree, i);

        if(!node) {
            logging::warn("project::open: tree[%zu] is not a valid JSON object", i);
            continue;
        }

        const unsigned long id = json_object_get_number(node, "id");
        const JSON_Array *inputs = json_object_get_array(node, "inputs");

        if(BaseNode *node = project::node_by_id(id)) {
            node->inputs.resize(json_array_get_count(inputs), nullptr);

            for(std::size_t j = 0; j < node->inputs.size(); ++j) {
                if(BaseNode *input = project::node_by_id(json_array_get_number(inputs, j))) {
                    node->inputs[j] = input;
                    input->outputs.insert(node);
                }
                else {
                    logging::warn("project::open: tree[%zu].inputs[%zu] refers to an unknown node", i, j);
                    continue;
                }
            }
        }
        else {
            logging::warn("project::open: tree[%zu].id refers to an unknown node", i);
            logging::warn("project::open: if you see this message, the code has fucked up");
            continue;
        }
    }

    json_value_free(jsonv);
    return true;
}

static bool write_json(void)
{
    JSON_Value *jsonv = json_value_init_object();
    JSON_Object *json = json_value_get_object(jsonv);

    json_object_set_string(json, "description", project::description.c_str());

    switch(input::mode) {
        case INPUT_MODE_DIRECTORY_ITERATOR:
            json_object_set_string(json, "input.mode", "directory_iterator");
            break;
        case INPUT_MODE_STD_SPRINTF:
            json_object_set_string(json, "input.mode", "std_sprintf");
            break;
        default:
            json_object_set_string(json, "input.mode", "disabled");
            break;
    }

    json_object_set_string(json, "input.directory", input::directory.generic_string().c_str());
    json_object_set_string(json, "input.format", input::format.c_str());

    JSON_Value *treev = json_value_init_array();
    JSON_Array *tree = json_value_get_array(treev);

    for(BaseNode *node : project::tree) {
        JSON_Value *nodev = json_value_init_object();
        JSON_Object *obj = json_value_get_object(nodev);

        JSON_Value *posv = json_value_init_array();
        JSON_Array *pos = json_value_get_array(posv);
        json_array_append_number(pos, node->position.x);
        json_array_append_number(pos, node->position.y);

        JSON_Value *inputsv = json_value_init_array();
        JSON_Array *inputs = json_value_get_array(inputsv);

        for(BaseNode *input : node->inputs) {
            if(input == nullptr)
                json_array_append_number(inputs, ULONG_MAX);
            else json_array_append_number(inputs, input->id);
        }

        json_object_set_number(obj, "id", node->id);
        json_object_set_string(obj, "name", node->name.c_str());
        json_object_set_value(obj, "inputs", inputsv);
        json_object_set_value(obj, "position", posv);
        const NodeType type = node->get_type();

        if(type == NODE_DEST_DISPLAY) {
            DestDisplayNode *dest_display = static_cast<DestDisplayNode *>(node);
            json_object_set_string(obj, "type", "dest_display");
            json_object_set_boolean(obj, "always_render", dest_display->always_render);
            json_array_append_value(tree, nodev);
            continue;
        }

        if(type == NODE_DEST_IMAGE) {
            DestImageNode *dest_image = static_cast<DestImageNode *>(node);
            json_object_set_string(obj, "type", "dest_image");
            json_array_append_value(tree, nodev);
            continue;
        }

        if(type == NODE_GLSL_SHADER) {
            GLSLShaderNode *glsl_shader = static_cast<GLSLShaderNode *>(node);
            json_object_set_string(obj, "type", "glsl_shader");
            json_object_set_string(obj, "shader", glsl_shader->shader_path.c_str());

            JSON_Value *texturesizev = json_value_init_array();
            JSON_Array *texturesize = json_value_get_array(texturesizev);
            json_array_append_number(texturesize, glsl_shader->texture_width);
            json_array_append_number(texturesize, glsl_shader->texture_height);
            json_object_set_value(obj, "texture_size", texturesizev);

            JSON_Value *paramsv = json_value_init_array();
            JSON_Array *params = json_value_get_array(paramsv);

            for(float param : glsl_shader->params) {
                json_array_append_number(params, param);
            }

            json_object_set_value(obj, "params", paramsv);

            json_array_append_value(tree, nodev);
            continue;
        }

        if(type == NODE_SHADERTOY) {
            ShadertoyNode *shadertoy = static_cast<ShadertoyNode *>(node);
            json_object_set_string(obj, "type", "shadertoy");
            json_object_set_string(obj, "shader", shadertoy->shader_path.c_str());

            JSON_Value *texturesizev = json_value_init_array();
            JSON_Array *texturesize = json_value_get_array(texturesizev);
            json_array_append_number(texturesize, shadertoy->texture_width);
            json_array_append_number(texturesize, shadertoy->texture_height);
            json_object_set_value(obj, "texture_size", texturesizev);

            json_array_append_value(tree, nodev);
            continue;
        }

        if(type == NODE_SRC_BLANK) {
            SrcBlankNode *src_blank = static_cast<SrcBlankNode *>(node);
            json_object_set_string(obj, "type", "src_blank");

            JSON_Value *colorv = json_value_init_array();
            JSON_Array *color = json_value_get_array(colorv);
            json_array_append_number(color, src_blank->color.x);
            json_array_append_number(color, src_blank->color.y);
            json_array_append_number(color, src_blank->color.z);
            json_array_append_number(color, src_blank->color.w);
            json_object_set_value(obj, "color", colorv);

            json_array_append_value(tree, nodev);
            continue;
        }

        if(type == NODE_SRC_IMAGE) {
            SrcImageNode *src_image = static_cast<SrcImageNode *>(node);
            json_object_set_string(obj, "type", "src_image");
            json_object_set_boolean(obj, "enable_display", src_image->enable_display);
            json_array_append_value(tree, nodev);
            continue;
        }

        logging::warn("project::save: unknown node type %u", type);
        json_value_free(nodev);
    }

    json_object_set_value(json, "tree", treev);

    std::ofstream stream = std::ofstream(project::json_path, std::ios::out | std::ios::trunc);

    if(!stream.is_open()) {
        logging::warn("project::save: can't open %s", project::json_path.generic_string().c_str());
        json_value_free(jsonv);
        return false;
    }

    if(json_serialize_to_file(jsonv, project::json_path.string().c_str()) != JSONSuccess) {
        logging::warn("project::save: %s: json write failure", project::json_path.generic_string().c_str());
        json_value_free(jsonv);
        return false;
    }

    json_value_free(jsonv);
    return true;
}

void project::open(const std::filesystem::path &directory)
{
    if(std::filesystem::is_directory(directory)) {
        project::directory = directory;
        project::json_path = std::filesystem::path(project::directory / "riteg.json");
        project::imgui_ini_path = std::filesystem::path(project::directory / "imgui.ini").string();
        ImGui::GetIO().IniFilename = project::imgui_ini_path.c_str();

        project::tree.clear();

        if(parse_json()) {
            logging::info("project::open: opened %s", directory.generic_string().c_str());
            return;
        }
        else {
            logging::warn("project::open: %s: json parse failure", directory.generic_string().c_str());
            project::close();
            return;
        }
    }

    logging::warn("project::open: %s is not a directory", directory.generic_string().c_str());
    project::close();
}

void project::close(void)
{
    project::directory = std::filesystem::path();
    project::json_path = std::filesystem::path();
    project::imgui_ini_path = std::string();
    ImGui::GetIO().IniFilename = nullptr;

    project::tree.clear();
}

void project::save(void)
{
    if(project::directory.empty()) {
        logging::warn("project:save: no project open");
        return;
    }

    if(write_json()) {
        logging::info("project:save: saved %s", project::directory.generic_string().c_str());
        return;
    }

    logging::warn("project:save: %s: json write failure", project::directory.generic_string().c_str());
}

BaseNode *project::node_by_id(unsigned long id)
{
    for(BaseNode *node : project::tree) {
        if(node->id != id)
            continue;
        return node;
    }

    return nullptr;
}

void project::restore_layout(void)
{
    ImGui::DockBuilderRemoveNodeChildNodes(globals::dockspace_id);

    ImGuiID left = ImGui::DockBuilderSplitNode(globals::dockspace_id, ImGuiDir_Left, 0.20f, nullptr, &globals::dockspace_id);
    ImGuiID left_down = ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.50f, nullptr, &left);

    ImGui::DockBuilderDockWindow("###ProjectEdit_Window", left);
    ImGui::DockBuilderDockWindow("###FrameSelect_Window", left_down);
    ImGui::DockBuilderDockWindow("###NodeEdit_Window", globals::dockspace_id);
    ImGui::DockBuilderFinish(globals::dockspace_id);
}
