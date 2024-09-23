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
    "${CMAKE_CURRENT_LIST_DIR}/riteg/globals.cc"
    "${CMAKE_CURRENT_LIST_DIR}/riteg/logging.cc"
    "${CMAKE_CURRENT_LIST_DIR}/riteg/main.cc"
    "${CMAKE_CURRENT_LIST_DIR}/riteg/menu_bar.cc")
target_compile_definitions(riteg PRIVATE GLFW_INCLUDE_NONE)
target_include_directories(riteg PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries(riteg PRIVATE glad glfw imgui imgui_glfw imgui_opengl3)
target_precompile_headers(riteg PRIVATE "${CMAKE_CURRENT_LIST_DIR}/riteg/pch.hh")
