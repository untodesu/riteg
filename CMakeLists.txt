cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(riteg LANGUAGES C CXX VERSION 2.5.1)

## If possible, enable solution directories; this allows
## built-in pseudotargets like ALL_BUILD and ZERO_CHECK to
## be moved out of sight into a separate directory
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

## Ensure we are never building dependencies as dynamic
## libraries; we only use dynamic libraries for non-compiled
## third-party dependencies (such as SDL3 and maybe Discord-RPC);
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)

## Microsoft-vendored STL has been permissively licensed for quite
## a while, so it makes all the sense to statically link with it to
## avoid pulling redistributable installers alongside the game package
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

find_package(glfw3 3.3 QUIET)

if(NOT glfw3_FOUND)
    include(FetchContent)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.3)
    FetchContent_MakeAvailable(glfw)
endif()

add_subdirectory(deps/glad)
add_subdirectory(deps/lua)
add_subdirectory(deps/stb)
add_subdirectory(riteg)

install(FILES "${CMAKE_CURRENT_LIST_DIR}/LICENSE.txt" DESTINATION ".")
install(FILES "${CMAKE_CURRENT_LIST_DIR}/thirdpartylegalnotices.txt" DESTINATION ".")
install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/project" DESTINATION ".")

set(CPACK_PACKAGE_NAME "RITEG")
set(CPACK_PACKAGE_VENDOR "untodesu")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Shader-based VHS filter host application")
set(CPACK_PACKAGE_CONTACT "https://github.com/untodesu/riteg/issues")

set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION_TWEAK ${PROJECT_VERSION_TWEAK})

set(CPACK_PACKAGE_DIRECTORY "${PROJECT_BINARY_DIR}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${CPACK_PACKAGE_NAME}")

include(CPack)
