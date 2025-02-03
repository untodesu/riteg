add_library(toml STATIC "${CMAKE_CURRENT_LIST_DIR}/toml.c")
target_include_directories(toml PUBLIC "${DEPS_INCLUDE_DIR}")
set_target_properties(toml PROPERTIES FOLDER deps)

if(WIN32)
    target_compile_definitions(toml PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
