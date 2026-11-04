cmake_minimum_required(VERSION 4.0.2)

project(flatcode VERSION 0.0.1)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "Applying linux-specific settings")
    set(GLFW_BUILD_WAYLAND ON CACHE BOOL "" FORCE)
    set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)
endif()

add_subdirectory(external/raylib)
add_subdirectory(Runtime/)
add_subdirectory(Base/)
add_subdirectory(Renderer/)