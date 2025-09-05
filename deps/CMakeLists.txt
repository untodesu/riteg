add_subdirectory(glad)
add_subdirectory(glfw)
add_subdirectory(imgui)
add_subdirectory(lua)
add_subdirectory(stb)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_custom_target(thirdparty ALL
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_LIST_DIR}/thirdparty.py" "${CMAKE_CURRENT_LIST_DIR}"
    BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/thirdparty.txt"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/thirdparty.py"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/thirdparty.txt" DESTINATION ".")
