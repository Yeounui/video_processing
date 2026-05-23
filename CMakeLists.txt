cmake_minimum_required(VERSION 3.16)
project(video_processing C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(LIBEDIT REQUIRED IMPORTED_TARGET libedit)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale libavdevice)
find_package(OpenGL REQUIRED)

# STB headers are gitignored; fall back to the main working tree if absent locally
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb/stb_image.h")
    set(STB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../third_party/stb")
else()
    set(STB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb")
endif()

add_executable(video_processing
    src/main.c
    src/input.c
    src/gl_loader.c
    src/renderer.c
    src/image_processor.c
    src/image_saver.c
    src/event_handler.c
)

target_include_directories(video_processing PRIVATE
    src
    include
    ${STB_INCLUDE_DIR}
)

target_link_libraries(video_processing PRIVATE
    PkgConfig::SDL2
    PkgConfig::LIBEDIT
    PkgConfig::FFMPEG
    OpenGL::GL
    m
)

add_executable(test_cpu_reference
    tests/test_cpu_reference.c
    src/image_processor.c
    src/gl_loader.c
)
target_include_directories(test_cpu_reference PRIVATE include)
target_link_libraries(test_cpu_reference PRIVATE PkgConfig::SDL2 OpenGL::GL m)

add_executable(test_image_saver
    tests/test_image_saver.c
    src/image_saver.c
)
target_include_directories(test_image_saver PRIVATE include ${STB_INCLUDE_DIR})
target_link_libraries(test_image_saver PRIVATE m)

add_executable(test_glsl_parity
    tests/test_glsl_parity.c
    src/image_processor.c
    src/gl_loader.c
)
target_include_directories(test_glsl_parity PRIVATE include ${STB_INCLUDE_DIR})
target_link_libraries(test_glsl_parity PRIVATE PkgConfig::SDL2 OpenGL::GL m)
