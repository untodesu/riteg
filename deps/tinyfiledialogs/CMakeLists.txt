cmake_minimum_required(VERSION 3.15)
project(TINYFILEDIALOGS LANGUAGES C VERSION 3.18.2)
add_library(tfd STATIC "${CMAKE_CURRENT_LIST_DIR}/src/tinyfiledialogs.c")
target_include_directories(tfd PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")

if(WIN32)
    target_compile_definitions(tfd PRIVATE comdlg32 ole32)
endif()
