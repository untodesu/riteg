#include "riteg/pch.hh"

#include "riteg/cmdline.hh"
#include "riteg/loader.hh"
#include "riteg/loader_fsiter.hh"
#include "riteg/loader_oneshot.hh"
#include "riteg/loader_sprintf.hh"
#include "riteg/program.hh"
#include "riteg/project.hh"
#include "riteg/saver.hh"
#include "riteg/saver_oneshot.hh"
#include "riteg/saver_sprintf.hh"
#include "riteg/source.hh"
#include "riteg/timings.hh"

int main(int argc, char** argv)
{
    logging::add_sink(&logging::sinks::stderr_ansi);

    glfwSetErrorCallback([](int error, const char* description) {
        LOG_ERROR("glfw error: {}: {}", error, description);
    });

    riteg_force_assert_msg(glfwInit(), "glfwInit failed");

    auto window = glfwCreateWindow(640, 480, "RITEG", nullptr, nullptr);
    riteg_force_assert_msg(window, "glfwCreateWindow failed");

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    riteg_force_assert_msg(gladLoadGL(&glfwGetProcAddress), "gladLoadGL failed");

    Loader* loader = nullptr;
    Saver* saver = nullptr;

    cmdline::init(argc, argv);

    double target_fps = 0.0;
    if(auto value = cmdline::get("fps"); value.size()) {
        auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), target_fps);

        if(error != std::errc() || end != value.data() + value.size() || target_fps < 0.0) {
            LOG_CRITICAL("invalid -fps value: {}", value);
            cmdline::deinit();
            glfwDestroyWindow(window);
            glfwTerminate();
            return EXIT_FAILURE;
        }
    }

    if(target_fps > 0.0) {
        LOG_INFO("target fps: {:.3f}", target_fps);
    }

    auto loader_type = cmdline::get("loader");
    auto saver_type = cmdline::get("saver");

    if(loader_type.size()) {
        if(loader_type == "fsiter") {
            loader = new Loader_FSIter;
        }
        else if(loader_type == "oneshot") {
            loader = new Loader_OneShot;
        }
        else if(loader_type == "sprintf") {
            loader = new Loader_Sprintf;
        }
        else {
            LOG_CRITICAL("unknown loader type: {}", loader_type);
            std::terminate();
        }
    }

    if(saver_type.size()) {
        if(saver_type == "oneshot") {
            saver = new Saver_OneShot;
        }
        else if(saver_type == "sprintf") {
            saver = new Saver_Sprintf;
        }
        else {
            LOG_CRITICAL("unknown saver type: {}", saver_type);
            std::terminate();
        }
    }

    Program::init();

    project::init();

    cmdline::init_late();

    if(!project::finalize()) {
        delete loader;
        delete saver;
        project::deinit();
        Program::deinit();
        cmdline::deinit();
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    if(loader) {
        loader->init();
    }

    if(saver) {
        saver->init();
    }

    Timings timings;
    timings.current_time = glfwGetTime();
    timings.delta_time = 0.0f;
    timings.frame_count = 0;

    // Pace the whole loop, not just the render: a project authored per frame
    // (VHS.V9) should advance at its target rate, not at the display refresh
    // rate. 0 leaves pacing to VSync.
    const double frame_interval = target_fps > 0.0 ? 1.0 / target_fps : 0.0;
    double next_frame_time = glfwGetTime();

    while(!glfwWindowShouldClose(window)) {
        if(frame_interval > 0.0) {
            auto now = glfwGetTime();

            if(now < next_frame_time) {
                std::this_thread::sleep_for(std::chrono::duration<double>(next_frame_time - now));
                now = glfwGetTime();
            }

            next_frame_time += frame_interval;

            // Do not catch up after a slow frame: a burst of frames would
            // bring the shake right back.
            if(next_frame_time < now)
                next_frame_time = now;
        }

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

    Program::deinit();

    cmdline::deinit();

    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}
