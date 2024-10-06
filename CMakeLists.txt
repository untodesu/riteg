cmake_minimum_required(VERSION 3.15)
project(RITEG LANGUAGES CXX VERSION 2.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(deps/glad)
add_subdirectory(deps/imgui)
add_subdirectory(deps/imnodes)
add_subdirectory(deps/parson)
add_subdirectory(deps/stb)
add_subdirectory(deps/tinyfiledialogs)
find_package(glfw3 QUIET)

if(NOT glfw3_FOUND)
    include(FetchContent)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glfw3
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE)
    FetchContent_MakeAvailable(glfw3)
endif()

add_executable(riteg
    "${CMAKE_CURRENT_LIST_DIR}/riteg/graph/base_node.cc"
    "${CMAKE_CURRENT_LIST_DIR}/riteg/graph/dest_display.cc"
    "${CMAKE_CURRENT_LIST_DIR}/riteg/graph/dest_image.cc"
    "${CMAKE_CURRENT_LIST_DIR}/riteg/graph/glsl_shader.cc"
    "${CMAKE_CURRENT_LIST_DIR}/riteg/graph/shadertoy.cc"
    "${CMAKE_CURRENT_LIST_DIR}/riteg/graph/src_blank.cc"
    "${CMAKE_CURRENT_LIST_DIR}/riteg/graph/src_image.cc"
    "${CMAKE_CURRENT_LIST_DIR}/riteg/core/globals.cc"
    "${CMAKE_CURRENT_LIST_DIR}/riteg/core/logging.cc"
    "${CMAKE_CURRENT_LIST_DIR}/riteg/gui/frame_select.cc"
    "${CMAKE_CURRENT_LIST_DIR}/riteg/gui/menu_bar.cc"
    "${CMAKE_CURRENT_LIST_DIR}/riteg/gui/node_edit.cc"
    "${CMAKE_CURRENT_LIST_DIR}/riteg/gui/project_edit.cc"
    "${CMAKE_CURRENT_LIST_DIR}/riteg/gui/shader_files.cc"
    "${CMAKE_CURRENT_LIST_DIR}/riteg/gui/style.cc"
    "${CMAKE_CURRENT_LIST_DIR}/riteg/project/input.cc"
    "${CMAKE_CURRENT_LIST_DIR}/riteg/project/project.cc"
    "${CMAKE_CURRENT_LIST_DIR}/riteg/main.cc")
target_compile_definitions(riteg PRIVATE GLFW_INCLUDE_NONE)
target_include_directories(riteg PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries(riteg PRIVATE glad glfw imgui imgui_glfw imgui_opengl3 imnodes parson stb tfd)
target_precompile_headers(riteg PRIVATE "${CMAKE_CURRENT_LIST_DIR}/riteg/stdafx.hh")
