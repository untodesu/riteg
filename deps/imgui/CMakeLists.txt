add_library(imgui STATIC
    "${CMAKE_CURRENT_LIST_DIR}/include/imconfig.h"
    "${CMAKE_CURRENT_LIST_DIR}/include/imgui_stdlib.h"
    "${CMAKE_CURRENT_LIST_DIR}/include/imgui.h"
    "${CMAKE_CURRENT_LIST_DIR}/include/imstb_rectpack.h"
    "${CMAKE_CURRENT_LIST_DIR}/include/imstb_textedit.h"
    "${CMAKE_CURRENT_LIST_DIR}/include/imstb_truetype.h"
    "${CMAKE_CURRENT_LIST_DIR}/src/imgui_demo.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/imgui_draw.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/imgui_stdlib.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/imgui_internal.h"
    "${CMAKE_CURRENT_LIST_DIR}/src/imgui_tables.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/imgui_widgets.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/imgui.cpp")
target_include_directories(imgui PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
target_include_directories(imgui PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")

add_library(imgui_glfw STATIC
    "${CMAKE_CURRENT_LIST_DIR}/include/imgui_impl_glfw.h"
    "${CMAKE_CURRENT_LIST_DIR}/src/imgui_impl_glfw.cpp")
target_link_libraries(imgui_glfw PUBLIC glfw imgui)

add_library(imgui_opengl3 STATIC
    "${CMAKE_CURRENT_LIST_DIR}/include/imgui_impl_opengl3.h"
    "${CMAKE_CURRENT_LIST_DIR}/src/imgui_impl_opengl3.cpp")
target_link_libraries(imgui_opengl3 PUBLIC imgui)
