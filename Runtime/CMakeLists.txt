set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_executable(flatcode ${SOURCES})

target_link_libraries(flatcode PUBLIC Base Renderer)

# шрифт lucide и другие ассеты кладём рядом с исполняемым файлом
add_custom_command(TARGET flatcode POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/Assets $<TARGET_FILE_DIR:flatcode>/Assets)