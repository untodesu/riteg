#ifndef RITEG_PCH_HH
#define RITEG_PCH_HH 1
#pragma once

#include <cassert>
#include <cctype>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <queue>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>

#include <glad/gl.h>

#include <lua.hpp>

#include <stb_image.h>
#include <stb_image_write.h>

constexpr static const char *DEBUG_ANSI_INFO = "\033[1;32;92minfo:\033[0m ";
constexpr static const char *DEBUG_ANSI_WARNING = "\033[1;33;93mwarning:\033[0m ";
constexpr static const char *DEBUG_ANSI_FATAL = "\033[1;31;91mfatal:\033[0m ";

#ifdef NDEBUG
#define riteg_info std::cerr << DEBUG_ANSI_INFO
#define riteg_warning std::cerr << DEBUG_ANSI_WARNING
#define riteg_fatal std::cerr << DEBUG_ANSI_FATAL
#else
#define riteg_info std::cerr << "[" << std::filesystem::path(__FILE__).filename().string() << ":" << __LINE__ << "]: " << DEBUG_ANSI_INFO
#define riteg_warning std::cerr << "[" << std::filesystem::path(__FILE__).filename().string() << ":" << __LINE__ << "]: " << DEBUG_ANSI_WARNING
#define riteg_fatal std::cerr << "[" << std::filesystem::path(__FILE__).filename().string() << ":" << __LINE__ << "]: " << DEBUG_ANSI_FATAL
#endif

#define riteg_force_assert(condition)                                                                           \
    do {                                                                                                        \
        if(!(condition)) {                                                                                      \
            riteg_fatal << "assertion failed: " << #condition << std::endl;                                     \
            riteg_fatal << std::filesystem::path(__FILE__).filename().string() << ":" << __LINE__ << std::endl; \
            std::terminate();                                                                                   \
        }                                                                                                       \
    } while(false)

#define riteg_force_assert_msg(condition, message)                                                              \
    do {                                                                                                        \
        if(!(condition)) {                                                                                      \
            riteg_fatal << "assertion failed: " << #condition << "; " << (message) << std::endl;                \
            riteg_fatal << std::filesystem::path(__FILE__).filename().string() << ":" << __LINE__ << std::endl; \
            std::terminate();                                                                                   \
        }                                                                                                       \
    } while(false)

#endif /* RITEG_PCH_HH */
