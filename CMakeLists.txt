cmake_minimum_required(VERSION 3.12)
project(fraCtal C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-O2")

add_executable(fraCtal
        src/main.c
        src/mandelbrot.c
        src/complex.c
        src/lib/ppmimage/ppm.c
        src/lib/lodepng/lodepng.c)

target_link_libraries(fraCtal -lpthread)
