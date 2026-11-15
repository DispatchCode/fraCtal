#ifndef FRACTAL2_MANDELBROT_H
#define FRACTAL2_MANDELBROT_H

#include <stdint.h>
#include <stdatomic.h>

//#define  PALETTE

typedef struct square_coords {
    size_t h;
    size_t w;
    size_t max_iterations;
    atomic_size_t *next_row;

} _square_coords;

typedef struct _mandelbrot_info {
    _square_coords *coords;
    size_t w;
    size_t h;
    size_t n_quadrants;
} mandelbrot_info;


extern void init(size_t w, size_t h, size_t n_threads,size_t max_iterations,mandelbrot_info*);
extern void generation(mandelbrot_info, char* filename);

#endif //FRACTAL2_MANDELBROT_H
