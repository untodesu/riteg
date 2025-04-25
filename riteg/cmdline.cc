#include "riteg/pch.hh"
#include "riteg/cmdline.hh"

#include "riteg/project.hh"

static int s_argc;
static char **s_argv;

void cmdline::init(int argc, char **argv)
{
    assert(argc > 0);
    assert(argv != nullptr);

    s_argc = argc;
    s_argv = argv;

    if(s_argc < 2) {
        riteg_fatal << "usage: riteg <script> [options]" << std::endl;
        std::terminate();
    }

    // Skip the application name
    s_argc -= 1;
    s_argv += 1;
}

void cmdline::init_late(void)
{
    assert(s_argc > 0);
    assert(s_argv != nullptr);

    project::run_lua_script(s_argv[0]);

    // Skip the script name
    s_argc -= 1;
    s_argv += 1;
}

void cmdline::deinit(void)
{
    s_argc = 0;
    s_argv = nullptr;
}

bool cmdline::contains(const char *option)
{
    assert(option != nullptr);

    for(int i = 0; i < s_argc; ++i) {
        if(s_argv[i][0] != '-')
            continue;
        if(std::strcmp(&s_argv[i][1], option))
            continue;
        return true;
    }

    return false;
}

const char *cmdline::get(const char *option, const char *fallback)
{
    assert(option != nullptr);

    for(int i = 0; i < s_argc; ++i) {
        if(s_argv[i][0] != '-')
            continue;
        if(std::strcmp(&s_argv[i][1], option))
            continue;
        if(i + 1 >= s_argc)
            return fallback;
        return s_argv[i + 1];
    }

    return fallback;
}
