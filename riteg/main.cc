#include "riteg/pch.hh"

#include "riteg/cmdline.hh"
#include "riteg/loader_fsiter.hh"
#include "riteg/loader_oneshot.hh"
#include "riteg/loader_sprintf.hh"
#include "riteg/loader.hh"
#include "riteg/project.hh"
#include "riteg/saver_oneshot.hh"
#include "riteg/saver_sprintf.hh"
#include "riteg/saver.hh"
#include "riteg/shader.hh"
#include "riteg/source.hh"
#include "riteg/timings.hh"

int main(int argc, char **argv)
{
    glfwSetErrorCallback([](int error, const char *description) {
        riteg_warning << "GLFW error: " << error << ": " << description << std::endl;
    });

    riteg_force_assert_msg(glfwInit(), "glfwInit failed");

    auto window = glfwCreateWindow(640, 480, "RITEG", nullptr, nullptr);
    riteg_force_assert_msg(window, "glfwCreateWindow failed");

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    riteg_force_assert_msg(gladLoadGL(&glfwGetProcAddress), "gladLoadGL failed");

    Loader *loader = nullptr;
    Saver *saver = nullptr;

    cmdline::init(argc, argv);

    if(auto loader_type = cmdline::get("loader")) {
        if(!std::strcmp(loader_type, "fsiter")) {
            loader = new Loader_FSIter;
        }
        else if(!std::strcmp(loader_type, "oneshot")) {
            loader = new Loader_OneShot;
        }
        else if(!std::strcmp(loader_type, "sprintf")) {
            loader = new Loader_Sprintf;
        }
        else {
            riteg_fatal << "Unknown loader type: " << loader_type << std::endl;
            std::terminate();
        }
    }

    if(auto saver_type = cmdline::get("saver")) {
        if(!std::strcmp(saver_type, "oneshot")) {
            saver = new Saver_OneShot;
        }
        else if(!std::strcmp(saver_type, "sprintf")) {
            saver = new Saver_Sprintf;
        }
        else {
            riteg_fatal << "Unknown saver type: " << saver_type << std::endl;
            std::terminate();
        }
    }

    Shader::init();

    project::init();

    cmdline::init_late();

    if(loader) loader->init();
    if(saver) saver->init();

    Timings timings;
    timings.current_time = glfwGetTime();
    timings.delta_time = 0.0f;
    timings.frame_count = 0;

    while(!glfwWindowShouldClose(window)) {
        auto current_time = glfwGetTime();

        timings.delta_time = current_time - timings.current_time;
        timings.current_time = current_time;

        if(loader) {
            // Load the next frame
            loader->step();
        }

        project::render(timings);

        if(auto display_source = project::get_display_source()) {
            auto read_width = display_source->get_texture_width();
            auto read_height = display_source->get_texture_height();

            int write_width, write_height;
            glfwGetFramebufferSize(window, &write_width, &write_height);

            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, display_source->get_framebuffer());
            glBlitFramebuffer(0, 0, read_width, read_height, 0, 0, write_width, write_height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        }

        if(auto output_source = project::get_output_source()) {
            auto read_width = output_source->get_texture_width();
            auto read_height = output_source->get_texture_height();

            int write_width, write_height;
            glfwGetFramebufferSize(window, &write_width, &write_height);

            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, output_source->get_framebuffer());
            glBlitFramebuffer(0, 0, read_width, read_height, 0, 0, write_width, write_height, GL_COLOR_BUFFER_BIT, GL_LINEAR);

            if(saver) {
                saver->step();
            }
        }

        timings.frame_count += 1;

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    project::deinit();

    Shader::deinit();

    cmdline::deinit();

    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}
