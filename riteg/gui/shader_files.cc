// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/gui/shader_files.hh"
#include "riteg/project/project.hh"

static std::vector<std::string> shader_filenames = {};
static std::string *selected_filename = nullptr;
static std::string edit_filename = {};
static bool is_creating = false;
static bool is_editing = false;

static bool is_shader_file(const std::filesystem::path &path)
{
    if(std::filesystem::is_regular_file(path)) {
        const std::string extension = path.extension().string();
        if(!extension.compare(".frag"))
            return true;
        if(!extension.compare(".glsl"))
            return true;
        return false;
    }

    return false;
}

static void new_shader(void)
{
    shader_filenames.push_back("new_shader.frag");
    selected_filename = &shader_filenames.back();
    edit_filename = shader_filenames.back();
    is_creating = true;
    is_editing = true;
}

static void delete_shader(void)
{
    if(selected_filename) {
        std::filesystem::remove(project::directory / *selected_filename);
        shader_files::refresh();
    }
}

static void rename_shader(void)
{
    if(selected_filename) {
        edit_filename = *selected_filename;
        is_creating = false;
        is_editing = true;
    }
}

static void make_path_unique(std::filesystem::path &path)
{
    while(std::filesystem::exists(path)) {
        const auto extension = path.extension();
        const auto stem = path.stem();
        path.replace_filename(stem.generic_string() + "(1)" + extension.generic_string());
    }
}

static void fix_extension(std::filesystem::path &path)
{
    const std::string extension = path.extension().string();
    if(!extension.compare(".frag"))
        return;
    if(!extension.compare(".glsl"))
        return;
    path.replace_extension(".frag");
}

static void layout_shader_listbox(void)
{
    for(std::size_t i = 0; i < shader_filenames.size(); ++i) {
        bool is_selected = selected_filename && (selected_filename == &shader_filenames[i]);

        if(is_selected && is_editing) {
            ImDrawList *draw_list = ImGui::GetWindowDrawList();

            ImGui::InputText("###ShaderFiles_EditInput", &edit_filename);

            const ImVec2 &r_min = ImGui::GetItemRectMin();
            const ImVec2 &r_max = ImGui::GetItemRectMax();
            const ImU32 r_color = ImGui::GetColorU32(ImGuiCol_Border);
            const float r_rounding = ImGui::GetStyle().FrameRounding;
            const float r_border_size = ImGui::GetStyle().FrameBorderSize;
            draw_list->AddRect(r_min, r_max, r_color, r_rounding, ImDrawFlags_None, r_border_size);
            
            ImGui::SameLine();

            if(ImGui::Button("OK###ShaderFiles_OK", ImVec2(-1.0f, 0.0f)) || (!edit_filename.empty() && ImGui::IsKeyReleased(ImGuiKey_Enter))) {
                if(is_creating) {
                    std::filesystem::path path = project::directory / edit_filename;
                    make_path_unique(path);
                    fix_extension(path);

                    std::ofstream stream = std::ofstream(path);
                    stream << "#version 330 core" << std::endl;
                    stream.close();

                    shader_filenames[i] = std::filesystem::relative(path, project::directory).generic_string();
                    selected_filename = &shader_filenames[i];
                    is_creating = false;
                    is_editing = false;
                }
                else {
                    std::filesystem::path src = project::directory / shader_filenames[i];
                    std::filesystem::path dst = project::directory / edit_filename;

                    if(shader_filenames[i].compare(edit_filename)) {
                        make_path_unique(dst);
                        fix_extension(dst);

                        std::filesystem::rename(src, dst);
                    }

                    shader_filenames[i] = std::filesystem::relative(dst, project::directory).generic_string();
                    selected_filename = &shader_filenames[i];
                    is_editing = false;
                }
            }
        }
        else {
            if(ImGui::Selectable(shader_filenames[i].c_str(), is_selected)) {
                if(is_creating)
                    shader_filenames.pop_back();
                selected_filename = &shader_filenames[i];
                edit_filename = std::string();
                is_creating = false;
                is_editing = false;
            }
        }
    }
}

void shader_files::layout(void)
{
    if(!ImGui::Begin("Shader Files###ShaderFiles_Window")) {
        ImGui::End();
        return;
    }

    if(ImGui::Button(" + ###ShaderFiles_NewButton"))
        new_shader();
    ImGui::SameLine();

    if(ImGui::Button(" - ###ShaderFiles_DeleteButton"))
        delete_shader();
    ImGui::SameLine();


    if(ImGui::Button("rename###ShaderFiles_RenameButton"))
        rename_shader();
    ImGui::SameLine();

    if(ImGui::Button("refresh###ShaderFiles_Refresh"))
        refresh();
    ImGui::SameLine();

    if(ImGui::Button("copy###ShaderFiles_Copy"))
        ImGui::SetClipboardText(selected_filename->c_str());
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    if(ImGui::BeginListBox("###ShaderFiles_ListBox", ImVec2(-1.0f, -1.0f))) {
        layout_shader_listbox();
        ImGui::EndListBox();
    }

    if(selected_filename && is_editing && ImGui::IsKeyReleased(ImGuiKey_Escape)) {
        if(is_creating)
            shader_filenames.pop_back();
        edit_filename = std::string();
        is_creating = false;
        is_editing = false;
    }

    ImGui::End();
}

void shader_files::refresh(void)
{
    shader_filenames.clear();
    selected_filename = nullptr;
    edit_filename = std::string();
    is_creating = false;
    is_editing = false;

    for(const std::filesystem::path &path : std::filesystem::directory_iterator(project::directory)) {
        if(!is_shader_file(path))
            continue;
        shader_filenames.push_back(std::filesystem::relative(path, project::directory).generic_string());
    }

    std::sort(shader_filenames.begin(), shader_filenames.end());
}
