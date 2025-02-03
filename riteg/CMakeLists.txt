add_executable(riteg
    "${CMAKE_CURRENT_LIST_DIR}/basic_image.hh"
    "${CMAKE_CURRENT_LIST_DIR}/cmdline.cc"
    "${CMAKE_CURRENT_LIST_DIR}/cmdline.hh"
    "${CMAKE_CURRENT_LIST_DIR}/debug_out.cc"
    "${CMAKE_CURRENT_LIST_DIR}/debug_out.hh"
    "${CMAKE_CURRENT_LIST_DIR}/main.cc"
    "${CMAKE_CURRENT_LIST_DIR}/precompiled.hh"
    "${CMAKE_CURRENT_LIST_DIR}/project.cc"
    "${CMAKE_CURRENT_LIST_DIR}/project.hh"
    "${CMAKE_CURRENT_LIST_DIR}/shader_pass.cc"
    "${CMAKE_CURRENT_LIST_DIR}/shader_pass.hh"
    "${CMAKE_CURRENT_LIST_DIR}/timings.cc"
    "${CMAKE_CURRENT_LIST_DIR}/timings.hh")
target_compile_features(riteg PUBLIC cxx_std_17)
target_compile_definitions(riteg PUBLIC GLFW_INCLUDE_NONE)
target_include_directories(riteg PUBLIC "${DEPS_INCLUDE_DIR}")
target_include_directories(riteg PUBLIC "${PROJECT_SOURCE_DIR}")
target_precompile_headers(riteg PRIVATE "${CMAKE_CURRENT_LIST_DIR}/precompiled.hh")
target_link_libraries(riteg PUBLIC glad_opengl glfw3 stb toml)

if(WIN32)
    enable_language(RC)
    target_sources(riteg PRIVATE "${CMAKE_CURRENT_LIST_DIR}/riteg.rc")
endif()
