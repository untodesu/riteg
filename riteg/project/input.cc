// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/core/logging.hh"
#include "riteg/project/input.hh"
#include "riteg/project/project.hh"

InputMode input::mode = INPUT_MODE_NULL;
std::filesystem::path input::directory = {};
std::string input::format = {};

GLuint input::image = 0;
int input::image_width = 0;
int input::image_height = 0;

std::size_t input::path_index = 0;
std::vector<std::filesystem::path> input::paths = {};

static void update_paths_directory_iterator(void)
{
    const std::filesystem::path full_path = project::directory / input::directory;

    if(!std::filesystem::is_directory(full_path)) {
        logging::warn("input::update_paths: %s is not a directory", full_path.generic_string().c_str());
        return;
    }

    for(const std::filesystem::path &path : std::filesystem::directory_iterator(full_path)) {
        if(!std::filesystem::is_regular_file(path))
            continue;
        input::paths.push_back(path);
    }

    std::sort(input::paths.begin(), input::paths.end());
}

static void update_paths_std_sprintf(void)
{
    const std::filesystem::path full_path = project::directory / input::directory;
    const std::string full_format = full_path.string() + std::string(1, std::filesystem::path::preferred_separator) + input::format;

    if(!std::filesystem::is_directory(full_path)) {
        logging::warn("input::update_paths: %s is not a directory", full_path.generic_string().c_str());
        return;
    }

    std::size_t index = 0;
    char buffer[1024] = {};

    while(true) {
        std::snprintf(buffer, sizeof(buffer), full_format.c_str(), index++);
        const std::filesystem::path path = std::filesystem::path(buffer);

        if(std::filesystem::is_regular_file(path)) {
            input::paths.push_back(path);
            continue;
        }

        break;
    }
}

void input::reload_image(void)
{
    if(!input::image)
        glGenTextures(1, &input::image);
    glBindTexture(GL_TEXTURE_2D, input::image);

    if(input::path_index >= input::paths.size()) {
        logging::warn("input::reload_image: non-valid index");
        return;
    }

    const std::string path = input::paths[input::path_index].string();
    stbi_uc *data = stbi_load(path.c_str(), &input::image_width, &input::image_height, nullptr, STBI_rgb_alpha);

    if(!data) {
        logging::warn("input::reload_image: %s: %s", path.c_str(), stbi_failure_reason());
        return;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, input::image_width, input::image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
}

void input::update_paths(void)
{
    input::paths.clear();
    input::path_index = 0;

    switch(input::mode) {
        case INPUT_MODE_DIRECTORY_ITERATOR:
            update_paths_directory_iterator();
            break;
        case INPUT_MODE_STD_SPRINTF:
            update_paths_std_sprintf();
            break;
    }
}
