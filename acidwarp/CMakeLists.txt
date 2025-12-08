cmake_minimum_required(VERSION 3.10)
project(acidwarp C)

set(CMAKE_C_STANDARD 99)

# Collect all source files
set(SOURCES
    acidwarp.c
    bit_map.c
    lut.c
    palinit.c
    rolnfade.c
    warp_text.c
    renderer_gl.c
    effects_rgb.c
)

# Find SDL2
find_package(SDL2 REQUIRED)

add_executable(acidwarp ${SOURCES})

target_include_directories(acidwarp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${SDL2_INCLUDE_DIRS} .)
target_link_libraries(acidwarp SDL2 GL GLEW m)

# SDL2 and OpenGL will be added in the next step
