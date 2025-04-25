add_executable(riteg
    "${CMAKE_CURRENT_LIST_DIR}/blank.cc"
    "${CMAKE_CURRENT_LIST_DIR}/blank.hh"
    "${CMAKE_CURRENT_LIST_DIR}/cmdline.cc"
    "${CMAKE_CURRENT_LIST_DIR}/cmdline.hh"
    "${CMAKE_CURRENT_LIST_DIR}/image.cc"
    "${CMAKE_CURRENT_LIST_DIR}/image.hh"
    "${CMAKE_CURRENT_LIST_DIR}/loader_fsiter.cc"
    "${CMAKE_CURRENT_LIST_DIR}/loader_fsiter.hh"
    "${CMAKE_CURRENT_LIST_DIR}/loader_oneshot.cc"
    "${CMAKE_CURRENT_LIST_DIR}/loader_oneshot.hh"
    "${CMAKE_CURRENT_LIST_DIR}/loader_sprintf.cc"
    "${CMAKE_CURRENT_LIST_DIR}/loader_sprintf.hh"
    "${CMAKE_CURRENT_LIST_DIR}/loader.hh"
    "${CMAKE_CURRENT_LIST_DIR}/main.cc"
    "${CMAKE_CURRENT_LIST_DIR}/pch.hh"
    "${CMAKE_CURRENT_LIST_DIR}/project.cc"
    "${CMAKE_CURRENT_LIST_DIR}/project.hh"
    "${CMAKE_CURRENT_LIST_DIR}/saver_oneshot.cc"
    "${CMAKE_CURRENT_LIST_DIR}/saver_oneshot.hh"
    "${CMAKE_CURRENT_LIST_DIR}/saver_sprintf.cc"
    "${CMAKE_CURRENT_LIST_DIR}/saver_sprintf.hh"
    "${CMAKE_CURRENT_LIST_DIR}/saver.cc"
    "${CMAKE_CURRENT_LIST_DIR}/saver.hh"
    "${CMAKE_CURRENT_LIST_DIR}/shader_bits.hh"
    "${CMAKE_CURRENT_LIST_DIR}/shader.cc"
    "${CMAKE_CURRENT_LIST_DIR}/shader.hh"
    "${CMAKE_CURRENT_LIST_DIR}/source.hh"
    "${CMAKE_CURRENT_LIST_DIR}/timings.hh")
target_precompile_headers(riteg PRIVATE "${CMAKE_CURRENT_LIST_DIR}/pch.hh")
target_compile_definitions(riteg PRIVATE GLFW_INCLUDE_NONE)
target_include_directories(riteg PRIVATE "${PROJECT_SOURCE_DIR}")
target_link_libraries(riteg PRIVATE glfw glad lua stb)
target_compile_features(riteg PRIVATE cxx_std_17)

if(WIN32)
    enable_language(RC)
    target_sources(riteg PRIVATE "${CMAKE_CURRENT_LIST_DIR}/riteg.rc")
endif()

install(TARGETS riteg RUNTIME DESTINATION ".")
