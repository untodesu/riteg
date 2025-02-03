#include "riteg/precompiled.hh"

#include "riteg/cmdline.hh"
#include "riteg/debug_out.hh"
#include "riteg/project.hh"
#include "riteg/shader_pass.hh"
#include "riteg/timings.hh"

#if defined(_WIN32)
extern "C" __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
extern "C" __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
#endif

static void wrapped_main(int argc, char **argv)
{
    cmdline::init(argc, argv);

    glfwSetErrorCallback([](int error_code, const char *message) {
        debug_out << "GLFW error: " << message;
    });

    if(!glfwInit()) {
        throw std::runtime_error("glfwInit() failed");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 0);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif /* __APPLE__ */

    auto window = glfwCreateWindow(640, 480, "RITEG.V2", nullptr, nullptr);

    if(!window) {
        throw std::runtime_error("glfwCreateWindow() failed");
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if(!gladLoadGL(reinterpret_cast<GLADloadfunc>(&glfwGetProcAddress))) {
        throw std::runtime_error("gladLoadGL() failed");
    }

    if(GLAD_GL_KHR_debug) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback([](auto source, auto type, auto id, auto severity, auto length, auto message, auto param) {
            debug_out << message;
        }, nullptr);

        // NVIDIA drivers print additional buffer information
        // to the debug output that programmers might find useful.
        static const std::uint32_t ignore_nvidia_131185 = UINT32_C(131185);
        glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_OTHER, GL_DONT_CARE, 1, &ignore_nvidia_131185, GL_FALSE);
    }

    GLuint vaobj;
    glGenVertexArrays(1, &vaobj);

    ShaderPass::global_init();

    project::init(cmdline::get("project", "project/vhs"), cmdline::get("preset"));

    if(auto resolution = cmdline::get("size")) {
        int width, height;

        if(2 == std::sscanf(resolution, "%dx%d", &width, &height)) {
            glfwSetWindowSize(window, width, height);
        }
    }
    else {
        int width = project::output_image.resolution[0];
        int height = project::output_image.resolution[1];
        glfwSetWindowSize(window, width, height);
    }

    Timings timings;
    Timings::setup(timings);

    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        Timings::update(timings);

        glBindVertexArray(vaobj);

        project::render(window, timings);

        glfwSwapBuffers(window);
    }

    project::deinit();

    ShaderPass::global_deinit();

    glDeleteVertexArrays(1, &vaobj);

    glfwDestroyWindow(window);
    glfwTerminate();
}

int main(int argc, char **argv)
{
    try {
        wrapped_main(argc, argv);
        return EXIT_SUCCESS;
    }
    catch(const std::exception &ex) {
        std::cerr << ex.what() << std::endl;
        std::terminate();
    }
    catch(...) {
        std::cerr << "non-std::exception throw" << std::endl;
        std::terminate();
    }
}
