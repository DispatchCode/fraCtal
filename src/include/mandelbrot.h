#ifndef FRACTAL_MANDELBROT_H
#define FRACTAL_MANDELBROT_H

#include <stddef.h>
#include <stdint.h>

typedef enum fractal_kind {
    FRACTAL_MANDELBROT = 0,
    FRACTAL_JULIA = 1
} fractal_kind;

typedef enum fractal_backend {
    FRACTAL_BACKEND_CPU = 0,
    FRACTAL_BACKEND_OPENCL = 1
} fractal_backend;

typedef enum fractal_color_mode {
    FRACTAL_COLOR_ITERATION_GRADIENT = 0,
    FRACTAL_COLOR_AXIS_GRADIENT = 1
} fractal_color_mode;

typedef struct fractal_config {
    size_t width;
    size_t height;
    size_t max_iterations;
    size_t thread_count;
    int adaptive_iterations;
    int adaptive_resolution;
    fractal_backend backend;
    fractal_color_mode color_mode;
    fractal_kind kind;
    double escape_radius;
    double red_a;
    double red_b;
    double blue_a;
    double blue_b;
    size_t val_in;
    size_t val_out;
    int attenuation;
    double x_min;
    double x_max;
    double y_min;
    double y_max;
    double julia_re;
    double julia_im;
    uint8_t inside_color[3];
    uint8_t color_start[3];
    uint8_t color_end[3];
} fractal_config;

void fractal_config_defaults(fractal_config *config);

/* Allocates width * height RGBA bytes and stores the address in rgba_out. */
int fractal_render(const fractal_config *config, uint8_t **rgba_out);
size_t fractal_effective_iterations(const fractal_config *config);
int fractal_backend_available(fractal_backend backend,
                              char *device_name, size_t device_name_size);

unsigned fractal_save_png(const char *filename, const uint8_t *rgba,
                          size_t width, size_t height);

#endif
