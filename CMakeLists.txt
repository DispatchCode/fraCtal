cmake_minimum_required(VERSION 3.12)
project(fraCtal C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
find_package(Threads REQUIRED)
find_package(OpenCL QUIET)

find_path(NUKLEAR_INCLUDE_DIR nuklear.h PATH_SUFFIXES nuklear)
find_path(NUKLEAR_SDL_RENDERER_INCLUDE_DIR nuklear_sdl_renderer.h
    HINTS ${NUKLEAR_INCLUDE_DIR}
    PATH_SUFFIXES nuklear demo/sdl_renderer)
if(NOT NUKLEAR_INCLUDE_DIR OR NOT NUKLEAR_SDL_RENDERER_INCLUDE_DIR)
    message(FATAL_ERROR
        "Nuklear headers not found. Install nuklear.h and nuklear_sdl_renderer.h, "
        "or set NUKLEAR_INCLUDE_DIR and NUKLEAR_SDL_RENDERER_INCLUDE_DIR.")
endif()

add_executable(fraCtal
    src/main.c
    src/gui.c
    src/gui_backend.c
    src/mandelbrot.c
    src/opencl_renderer.c
    src/complex.c
    src/lib/lodepng/lodepng.c)

target_include_directories(fraCtal PRIVATE
    src
    ${NUKLEAR_INCLUDE_DIR}
    ${NUKLEAR_SDL_RENDERER_INCLUDE_DIR})
target_compile_definitions(fraCtal PRIVATE
    NK_INCLUDE_FIXED_TYPES
    NK_INCLUDE_STANDARD_IO
    NK_INCLUDE_STANDARD_VARARGS
    NK_INCLUDE_DEFAULT_ALLOCATOR
    NK_INCLUDE_VERTEX_BUFFER_OUTPUT
    NK_INCLUDE_FONT_BAKING
    NK_INCLUDE_DEFAULT_FONT)
target_link_libraries(fraCtal PRIVATE PkgConfig::SDL2 Threads::Threads m)
if(OpenCL_FOUND)
    target_compile_definitions(fraCtal PRIVATE FRACTAL_HAVE_OPENCL)
    target_link_libraries(fraCtal PRIVATE OpenCL::OpenCL)
    message(STATUS "OpenCL support enabled (runtime GPU detection is performed at launch)")
else()
    message(STATUS "OpenCL development files not found; CPU rendering remains available")
endif()
target_compile_options(fraCtal PRIVATE -Wall -Wextra -Wpedantic)
