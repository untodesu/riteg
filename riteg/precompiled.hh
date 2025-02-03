#ifndef RITEG_PRECOMPILED_HH
#define RITEG_PRECOMPILED_HH 1
#pragma once

#include <cinttypes>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <GLFW/glfw3.h>

#include <glad/gl.h>

#include <stb_image.h>
#include <stb_image_write.h>
#include <stb_sprintf.h>

#include <toml.h>

#ifdef _WIN32
#include <windows.h>
#endif

using Vec3f = std::array<float, 3>;

constexpr static int MAX_SHADER_CHANNELS = 4; // Shadertoy compat
constexpr static int MAX_SHADER_PARAMETERS = 16; // RITEG.V1 compat

#endif /* RITEG_PRECOMPILED_HH */
