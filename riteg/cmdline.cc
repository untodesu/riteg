#include "riteg/pch.hh"

#include "riteg/cmdline.hh"

#include "riteg/project.hh"

namespace
{
int s_argc;
char** s_argv;
} // namespace

void cmdline::init(int argc, char** argv)
{
    assert(argc > 0);
    assert(argv != nullptr);

    s_argc = argc;
    s_argv = argv;

    if(s_argc < 2) {
        LOG_CRITICAL("usage: riteg <script> [options]");
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

bool cmdline::contains(std::string_view option)
{
    assert(option.size());

    for(int i = 0; i < s_argc; ++i) {
        std::string_view argument(s_argv[0]);

        if(argument.empty() || argument[0] != '-') {
            // Not an option
            continue;
        }

        if(0 == option.compare(argument.substr(1))) {
            return true;
        }
    }

    return false;
}

std::string_view cmdline::get(std::string_view option, std::string_view fallback)
{
    assert(option.size());

    for(int i = 0; i < s_argc; ++i) {
        std::string_view argument(s_argv[i]);

        if(argument.empty() || argument[0] != '-') {
            // Not an option
            continue;
        }

        if(0 == option.compare(argument.substr(1))) {
            if(i + 1 < s_argc) {
                return s_argv[i + 1];
            }

            return fallback;
        }
    }

    return fallback;
}

const char* cmdline::get_cstr(const char* option, const char* fallback)
{
    assert(option);

    for(int i = 0; i < s_argc; ++i) {
        if(s_argv[i][0] != '-') {
            // Not an option
            continue;
        }

        if(0 == std::strcmp(&s_argv[i][1], option)) {
            if(i + 1 < s_argc) {
                return s_argv[i + 1];
            }

            return fallback;
        }
    }

    return fallback;
}
