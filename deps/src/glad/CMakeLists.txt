add_library(glad_opengl STATIC "${CMAKE_CURRENT_LIST_DIR}/gl.c")
target_include_directories(glad_opengl PUBLIC "${DEPS_INCLUDE_DIR}")
set_target_properties(glad_opengl PROPERTIES FOLDER deps)
