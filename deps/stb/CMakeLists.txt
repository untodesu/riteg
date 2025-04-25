add_library(stb STATIC
    "${CMAKE_CURRENT_LIST_DIR}/src/stb_impl.c"
    "${CMAKE_CURRENT_LIST_DIR}/include/stb_image.h"
    "${CMAKE_CURRENT_LIST_DIR}/include/stb_image_write.h")
target_include_directories(stb PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")
set_target_properties(stb PROPERTIES FOLDER DEPS)
