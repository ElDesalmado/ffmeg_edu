cmake_minimum_required(VERSION 3.7 FATAL_ERROR)

project(ffmpeg_edu LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(FFMPEG COMPONENTS avformat avdevice REQUIRED)

add_executable(Chapter0 chapter_0.cpp)
target_link_libraries(Chapter0
        PRIVATE
        FFMPEG::avformat
        FFMPEG::avdevice
        )

target_include_directories(Chapter0
        PUBLIC
        3rdparty/stb/include
        )

# this MUST work
add_executable(hello_world_0 hello_world_0.c)
target_link_libraries(hello_world_0
        PRIVATE
        FFMPEG::avformat
        FFMPEG::avdevice
        )

add_custom_target(COPY_RESOURCES ALL
        COMMAND
        ${CMAKE_COMMAND} -E copy_if_different small_bunny_1080p_60fps.mp4 ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Copying resources to ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
        )