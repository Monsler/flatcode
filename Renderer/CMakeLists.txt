set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(Renderer ${SOURCES})
target_include_directories(Renderer PUBLIC include/)
target_link_libraries(Renderer PUBLIC raylib)