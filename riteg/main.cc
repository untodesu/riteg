// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2024, untodesu
#include "riteg/pch.hh"
#include "riteg/globals.hh"
#include "riteg/logging.hh"
#include "riteg/menu_bar.hh"
#include "riteg/style.hh"

#if defined(_WIN32)
extern "C" __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
extern "C" __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
#endif

static void on_glfw_error(int code, const char *message)
{
    logging::warn("glfw: %d: %s", code, message);
}

static void on_opengl_message(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *param)
{
    logging::info("opengl: %s", message);
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

    g_window = glfwCreateWindow(1280, 720, "RITEG V2", nullptr, nullptr);

    if(!g_window) {
        logging::crit("glfw: failed to open a window");
        std::terminate();
    }

    glfwSetWindowSizeLimits(g_window, 720, 480, GLFW_DONT_CARE, GLFW_DONT_CARE);

    glfwMakeContextCurrent(g_window);
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

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(g_window, true);
    ImGui_ImplOpenGL3_Init(nullptr);

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    style::apply();

    // Dummy values for visual clutter
    g_curframe = 42;
    g_numframes = 1800;

    while(!glfwWindowShouldClose(g_window)) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClearColor(0.000f, 0.000f, 0.100f, 1.000f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

        menu_bar::render();

        if(ImGui::Begin("Style editor"))
            ImGui::ShowStyleEditor();
        ImGui::End();

        ImGui::ShowDemoWindow();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(g_window);
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(g_window);
    glfwTerminate();

    logging::deinit();

    return 0;
}
