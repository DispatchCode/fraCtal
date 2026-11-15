#ifndef FRACTAL_OPENCL_RENDERER_H
#define FRACTAL_OPENCL_RENDERER_H

#include <stddef.h>
#include <stdint.h>

#include "mandelbrot.h"

int fractal_opencl_available(char *device_name, size_t device_name_size);
int fractal_opencl_render(const fractal_config *config, uint8_t **rgba_out,
                          char *error_message, size_t error_message_size);
void fractal_opencl_shutdown(void);

#endif
