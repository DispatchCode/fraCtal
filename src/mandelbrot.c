#include <pthread.h>
#include <math.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "include/complex.h"
#include "include/mandelbrot.h"
#include "include/opencl_renderer.h"
#include "lib/lodepng/lodepng.h"

#define ROWS_PER_TASK 8

typedef struct render_job {
    const fractal_config *config;
    uint8_t *rgba;
    size_t iteration_limit;
    atomic_size_t next_row;
} render_job;

static size_t pixel_offset(size_t x, size_t y, size_t width)
{
    return 4 * (x + y * width);
}

static uint8_t mix_channel(uint8_t start, uint8_t end, double amount)
{
    double value = (double)start + ((double)end - (double)start) * amount;
    if (value < 0.0)
        value = 0.0;
    if (value > 255.0)
        value = 255.0;
    return (uint8_t)value;
}

static uint8_t clamp_channel(double value)
{
    if (!(value > 0.0))
        return 0;
    if (value >= 255.0)
        return 255;
    return (uint8_t)value;
}

static void *render_rows(void *opaque)
{
    render_job *job = opaque;
    const fractal_config *config = job->config;
    const double x_span = config->x_max - config->x_min;
    const double y_span = config->y_max - config->y_min;
    const double x_denominator = config->width > 1 ? (double)(config->width - 1) : 1.0;
    const double y_denominator = config->height > 1 ? (double)(config->height - 1) : 1.0;

    for (;;) {
        size_t start_y = atomic_fetch_add_explicit(&job->next_row, ROWS_PER_TASK,
                                                   memory_order_relaxed);
        if (start_y >= config->height)
            break;

        size_t end_y = start_y + ROWS_PER_TASK;
        if (end_y > config->height)
            end_y = config->height;

        for (size_t y = start_y; y < end_y; ++y) {
            double point_im = config->y_max - y_span * (double)y / y_denominator;

            for (size_t x = 0; x < config->width; ++x) {
                double point_re = config->x_min + x_span * (double)x / x_denominator;
                complex z;
                complex c;

                if (config->kind == FRACTAL_JULIA) {
                    z.re = point_re;
                    z.im = point_im;
                    c.re = config->julia_re;
                    c.im = config->julia_im;
                } else {
                    z.re = 0.0;
                    z.im = 0.0;
                    c.re = point_re;
                    c.im = point_im;
                }

                size_t iterations = 0;
                double magnitude_squared = 0.0;
                while (iterations < job->iteration_limit &&
                       magnitude_squared < config->escape_radius) {
                    z = complex_sum(complex_mul(z, z), c);
                    magnitude_squared = complex_abs(z);
                    ++iterations;
                }

                size_t offset = pixel_offset(x, y, config->width);
                if (config->color_mode == FRACTAL_COLOR_AXIS_GRADIENT) {
                    double red = config->red_a + (config->red_b - config->red_a) *
                                 (double)x / (double)config->width;
                    double blue = config->blue_a + (config->blue_b - config->blue_a) *
                                  (double)y / (double)config->height;
                    double green = 0.0;
                    if (iterations >= config->val_in) {
                        double numerator = (double)iterations + (double)config->val_in;
                        double denominator = (double)job->iteration_limit + (double)config->val_in;
                        green = 255.0 - (numerator / denominator) *
                                (255.0 - (double)config->val_out);
                    }
                    double attenuation = 1.0;
                    if (config->attenuation) {
                        double fraction = (double)iterations / (double)job->iteration_limit;
                        attenuation = 1.0 - fraction * fraction;
                    }
                    job->rgba[offset] = clamp_channel(red * attenuation);
                    job->rgba[offset + 1] = clamp_channel(green);
                    job->rgba[offset + 2] = clamp_channel(blue * attenuation);
                } else if (magnitude_squared >= config->escape_radius) {
                    double smooth_iterations = (double)iterations;
                    if (isfinite(magnitude_squared) && magnitude_squared > 1.0) {
                        double log_magnitude = 0.5 * log(magnitude_squared);
                        if (log_magnitude > 0.0) {
                            double correction = log(log_magnitude / log(2.0)) / log(2.0);
                            if (isfinite(correction))
                                smooth_iterations += 1.0 - correction;
                        }
                    }
                    double amount = smooth_iterations / (double)job->iteration_limit;
                    job->rgba[offset] = mix_channel(config->color_start[0], config->color_end[0], amount);
                    job->rgba[offset + 1] = mix_channel(config->color_start[1], config->color_end[1], amount);
                    job->rgba[offset + 2] = mix_channel(config->color_start[2], config->color_end[2], amount);
                } else {
                    job->rgba[offset] = config->inside_color[0];
                    job->rgba[offset + 1] = config->inside_color[1];
                    job->rgba[offset + 2] = config->inside_color[2];
                }
                job->rgba[offset + 3] = 255;
            }
        }
    }

    return NULL;
}

void fractal_config_defaults(fractal_config *config)
{
    if (config == NULL)
        return;

    config->width = 1280;
    config->height = 720;
    config->max_iterations = 1000;
    config->thread_count = 4;
    config->adaptive_iterations = 1;
    config->adaptive_resolution = 1;
    config->backend = FRACTAL_BACKEND_CPU;
    config->color_mode = FRACTAL_COLOR_ITERATION_GRADIENT;
    config->kind = FRACTAL_MANDELBROT;
    config->escape_radius = 4.0;
    config->red_a = 0.0;
    config->red_b = 255.0;
    config->blue_a = 0.0;
    config->blue_b = 255.0;
    config->val_in = 32;
    config->val_out = 0;
    config->attenuation = 1;
    config->x_min = -2.0;
    config->x_max = 1.0;
    config->y_min = -1.0;
    config->y_max = 1.0;
    config->julia_re = -0.8;
    config->julia_im = 0.156;
    config->inside_color[0] = 0;
    config->inside_color[1] = 0;
    config->inside_color[2] = 0;
    config->color_start[0] = 12;
    config->color_start[1] = 38;
    config->color_start[2] = 180;
    config->color_end[0] = 255;
    config->color_end[1] = 220;
    config->color_end[2] = 48;
}

size_t fractal_effective_iterations(const fractal_config *config)
{
    if (config == NULL || !config->adaptive_iterations || config->max_iterations == 0)
        return config != NULL ? config->max_iterations : 0;

    double x_span = config->x_max - config->x_min;
    double y_span = config->y_max - config->y_min;
    if (!(x_span > 0.0) || !(y_span > 0.0) ||
        !isfinite(x_span) || !isfinite(y_span))
        return config->max_iterations;

    double zoom_x = 3.0 / x_span;
    double zoom_y = 2.0 / y_span;
    double zoom = zoom_x > zoom_y ? zoom_x : zoom_y;
    if (!(zoom > 1.0) || !isfinite(zoom))
        return config->max_iterations;

    /* Add 25% of the base budget for every twofold zoom-in. */
    double extra = (double)config->max_iterations * 0.25 * log2(zoom);
    double result = (double)config->max_iterations + extra;
    if (result > 1000000.0)
        result = 1000000.0;
    return (size_t)result;
}

static int fractal_config_is_valid(const fractal_config *config)
{
    if (config == NULL || config->width == 0 || config->height == 0 ||
        config->max_iterations == 0 ||
        (config->backend == FRACTAL_BACKEND_CPU && config->thread_count == 0) ||
        !isfinite(config->x_min) || !isfinite(config->x_max) ||
        !isfinite(config->y_min) || !isfinite(config->y_max) ||
        config->x_min >= config->x_max || config->y_min >= config->y_max ||
        !isfinite(config->escape_radius) || config->escape_radius <= 0.0 ||
        (config->kind != FRACTAL_MANDELBROT && config->kind != FRACTAL_JULIA) ||
        (config->color_mode != FRACTAL_COLOR_ITERATION_GRADIENT &&
         config->color_mode != FRACTAL_COLOR_AXIS_GRADIENT) ||
        (config->backend != FRACTAL_BACKEND_CPU &&
         config->backend != FRACTAL_BACKEND_OPENCL))
        return 0;

    if (config->width > SIZE_MAX / config->height ||
        config->width * config->height > SIZE_MAX / 4)
        return 0;

    return config->backend != FRACTAL_BACKEND_OPENCL ||
           (config->width <= UINT_MAX && config->height <= UINT_MAX &&
            fractal_effective_iterations(config) <= UINT_MAX);
}

int fractal_backend_available(fractal_backend backend,
                              char *device_name, size_t device_name_size)
{
    if (device_name != NULL && device_name_size > 0)
        device_name[0] = '\0';
    switch (backend) {
    case FRACTAL_BACKEND_CPU:
        if (device_name != NULL && device_name_size > 0)
            snprintf(device_name, device_name_size, "CPU");
        return 1;
    case FRACTAL_BACKEND_OPENCL:
        return fractal_opencl_available(device_name, device_name_size);
    }
    return 0;
}

int fractal_render(const fractal_config *config, uint8_t **rgba_out)
{
    if (rgba_out == NULL)
        return -1;
    *rgba_out = NULL;

    if (!fractal_config_is_valid(config))
        return -1;

    if (config->backend == FRACTAL_BACKEND_OPENCL)
        return fractal_opencl_render(config, rgba_out, NULL, 0);

    uint8_t *rgba = calloc(config->width * config->height, 4);
    if (rgba == NULL)
        return -1;

    render_job job = {
        .config = config,
        .rgba = rgba,
        .iteration_limit = fractal_effective_iterations(config)
    };
    atomic_init(&job.next_row, 0);

    size_t worker_count = config->thread_count;
    if (worker_count > config->height)
        worker_count = config->height;

    pthread_t *threads = calloc(worker_count, sizeof(*threads));
    if (threads == NULL) {
        free(rgba);
        return -1;
    }

    size_t created = 0;
    for (size_t i = 0; i < worker_count; ++i) {
        if (pthread_create(&threads[created], NULL, render_rows, &job) == 0) {
            ++created;
        } else {
            render_rows(&job);
            break;
        }
    }

    for (size_t i = 0; i < created; ++i)
        pthread_join(threads[i], NULL);

    free(threads);
    *rgba_out = rgba;
    return 0;
}

unsigned fractal_save_png(const char *filename, const uint8_t *rgba,
                          size_t width, size_t height)
{
    if (filename == NULL || rgba == NULL || width == 0 || height == 0)
        return 84;
    return lodepng_encode32_file(filename, rgba, width, height);
}
