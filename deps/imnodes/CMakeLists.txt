cmake_minimum_required(VERSION 3.15)
project(IMNODES LANGUAGES CXX VERSION 1.0.0)

add_library(imnodes STATIC
    "${CMAKE_CURRENT_LIST_DIR}/src/imnodes.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/imnodes_ez.cpp")
target_include_directories(imnodes PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")
target_link_libraries(imnodes PUBLIC imgui)
