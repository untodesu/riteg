// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/stdafx.hh"
#include "riteg/core/globals.hh"
#include "riteg/core/logging.hh"
#include "riteg/graph/dest_display.hh"
#include "riteg/gui/frame_select.hh"
#include "riteg/gui/menu_bar.hh"
#include "riteg/gui/node_edit.hh"
#include "riteg/gui/project_edit.hh"
#include "riteg/gui/shader_files.hh"
#include "riteg/gui/style.hh"
#include "riteg/project/project.hh"

#include "font/anonymous-pro-regular.h"

#if defined(_WIN32)
extern "C" __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
extern "C" __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
#endif

constexpr static const char *vert_source = R"glsl(
    #version 330 core
    void main(void)
    {
        vec2 positions[4];
        positions[0] = vec2(-1.0, -1.0);
        positions[1] = vec2(-1.0, +1.0);
        positions[2] = vec2(+1.0, -1.0);
        positions[3] = vec2(+1.0, +1.0);
        gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
    }
)glsl";

static void on_glfw_error(int code, const char *message)
{
    logging::warn("glfw: %d: %s", code, message);
}

static void on_opengl_message(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *param)
{
    logging::info("opengl: %s", message);
}

static void create_vertex_shader(void)
{
    globals::vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(globals::vertex_shader, 1, &vert_source, nullptr);
    glCompileShader(globals::vertex_shader);

    GLint info_log_length = {};
    std::basic_string<GLchar> info_log = {};
    glGetShaderiv(globals::vertex_shader, GL_INFO_LOG_LENGTH, &info_log_length);

    if(info_log_length > 1) {
        info_log.resize(info_log_length, GLchar(0x00));
        glGetShaderInfoLog(globals::vertex_shader, info_log_length, nullptr, info_log.data());
        logging::warn("vert: %s", info_log.c_str());
    }

    GLint compile_status = {};
    glGetShaderiv(globals::vertex_shader, GL_COMPILE_STATUS, &compile_status);

    if(!compile_status) {
        logging::crit("vert: shader compile failed");
        std::terminate();
    }
}

int main(void)
{
    logging::init();

    glfwSetErrorCallback(&on_glfw_error);

    if(!glfwInit()) {
        logging::crit("glfw: init failed");
        std::terminate();
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_SAMPLES, 0);

    globals::window = glfwCreateWindow(1280, 720, "RITEG V2", nullptr, nullptr);

    if(!globals::window) {
        logging::crit("glfw: failed to open a window");
        std::terminate();
    }

    glfwSetWindowSizeLimits(globals::window, 720, 480, GLFW_DONT_CARE, GLFW_DONT_CARE);

    glfwMakeContextCurrent(globals::window);
    glfwSwapInterval(1);

    if(!gladLoadGL(&glfwGetProcAddress)) {
        logging::crit("opengl: failed to load function pointers");
        std::terminate();
    }

    if(GLAD_GL_KHR_debug) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(&on_opengl_message, nullptr);

        // NVIDIA drivers print additional buffer information
        // to the debug output that programmers might find useful.
        static const uint32_t ignore_nvidia_131185 = 131185;
        glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_OTHER, GL_DONT_CARE, 1, &ignore_nvidia_131185, GL_FALSE);
    }
    else {
        logging::warn("opengl: KHR_debug extension not supported");
        logging::warn("opengl: OpenGL errors will not be logged");
    }

    logging::info("opengl: GL_VERSION: %s", glGetString(GL_VERSION));
    logging::info("opengl: GL_RENDERER: %s", glGetString(GL_RENDERER));

    create_vertex_shader();
    glGenVertexArrays(1, &globals::vertex_array);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(globals::window, true);
    ImGui_ImplOpenGL3_Init(nullptr);

    globals::imnodes_ctx = ImNodes::Ez::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr;

    io.Fonts->Clear();
    io.Fonts->AddFontFromMemoryCompressedTTF(AnynymousProRegular_compressed_data, AnynymousProRegular_compressed_size, 18.0f);
    io.FontDefault = io.Fonts->Fonts[0];

    style::apply_imgui();
    style::apply_imnodes();

    node_edit::init();

    project::close();

    while(!glfwWindowShouldClose(globals::window)) {
        for(BaseNode *node : project::tree) {
            node->rendered = false;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClearColor(0.000f, 0.000f, 0.100f, 1.000f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        globals::dockspace_id = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

        menu_bar::layout();

        if(!project::directory.empty()) {
            frame_select::layout();
            project_edit::layout();
            shader_files::layout();

            node_edit::layout();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        for(BaseNode *node : project::tree) {
            if(node->get_type() == NODE_DEST_DISPLAY) {
                DestDisplayNode *dest_node = static_cast<DestDisplayNode *>(node);
                if(!dest_node->always_render)
                    continue;
                globals::render_list.insert(dest_node);
            }
        }

        for(BaseNode *node : globals::render_list)
            node->render();
        globals::render_list.clear();

        glfwSwapBuffers(globals::window);
        glfwPollEvents();
    }

    ImNodes::Ez::FreeContext(globals::imnodes_ctx);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &globals::vertex_array);
    glDeleteShader(globals::vertex_shader);

    glfwDestroyWindow(globals::window);
    glfwTerminate();

    logging::deinit();

    return 0;
}
