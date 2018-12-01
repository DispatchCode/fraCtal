cmake_minimum_required(VERSION 3.12)
project(FraCtal C)

set(CMAKE_C_STANDARD 11)

include_directories(src/include)
include_directories(src/lib/ppmimage/include)

add_executable(FraCtal
        src/lib/ppmimage/ppm.c
        src/complex.c
        src/fractal.c
        src/main_fractal.c)
