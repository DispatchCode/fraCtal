#include "include/opencl_renderer.h"

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#ifdef FRACTAL_HAVE_OPENCL
#define CL_TARGET_OPENCL_VERSION 120
#include <CL/cl.h>

#include "opencl_kernel.h"

static int has_extension(const char *extensions, const char *name)
{
    size_t name_length = strlen(name);
    const char *match = extensions;
    while ((match = strstr(match, name)) != NULL) {
        if ((match == extensions || match[-1] == ' ') &&
            (match[name_length] == '\0' || match[name_length] == ' '))
            return 1;
        match += name_length;
    }
    return 0;
}

static cl_device_id find_fp64_gpu(int *uses_amd_extension)
{
    cl_uint platform_count = 0;
    if (clGetPlatformIDs(0, NULL, &platform_count) != CL_SUCCESS || platform_count == 0)
        return NULL;

    cl_platform_id *platforms = calloc(platform_count, sizeof(*platforms));
    if (platforms == NULL || clGetPlatformIDs(platform_count, platforms, NULL) != CL_SUCCESS) {
        free(platforms);
        return NULL;
    }

    cl_device_id selected = NULL;
    for (cl_uint p = 0; p < platform_count && selected == NULL; ++p) {
        cl_uint device_count = 0;
        if (clGetDeviceIDs(platforms[p], CL_DEVICE_TYPE_GPU, 0, NULL, &device_count) != CL_SUCCESS ||
            device_count == 0)
            continue;

        cl_device_id *devices = calloc(device_count, sizeof(*devices));
        if (devices == NULL || clGetDeviceIDs(platforms[p], CL_DEVICE_TYPE_GPU,
                                               device_count, devices, NULL) != CL_SUCCESS) {
            free(devices);
            continue;
        }

        for (cl_uint d = 0; d < device_count && selected == NULL; ++d) {
            cl_device_fp_config fp64_config = 0;
            size_t extensions_size = 0;
            if (clGetDeviceInfo(devices[d], CL_DEVICE_DOUBLE_FP_CONFIG, sizeof(fp64_config),
                                &fp64_config, NULL) != CL_SUCCESS || fp64_config == 0 ||
                clGetDeviceInfo(devices[d], CL_DEVICE_EXTENSIONS, 0, NULL,
                                &extensions_size) != CL_SUCCESS || extensions_size == 0)
                continue;

            char *extensions = malloc(extensions_size);
            if (extensions == NULL)
                continue;
            if (clGetDeviceInfo(devices[d], CL_DEVICE_EXTENSIONS, extensions_size,
                                extensions, NULL) == CL_SUCCESS) {
                if (has_extension(extensions, "cl_khr_fp64")) {
                    selected = devices[d];
                    *uses_amd_extension = 0;
                } else if (has_extension(extensions, "cl_amd_fp64")) {
                    selected = devices[d];
                    *uses_amd_extension = 1;
                }
            }
            free(extensions);
        }
        free(devices);
    }

    free(platforms);
    return selected;
}

static void copy_device_name(cl_device_id device, char *name, size_t name_size)
{
    if (name == NULL || name_size == 0)
        return;
    name[0] = '\0';
    size_t required = 0;
    if (clGetDeviceInfo(device, CL_DEVICE_NAME, 0, NULL, &required) != CL_SUCCESS || required == 0)
        return;
    char *device_name = malloc(required);
    if (device_name == NULL)
        return;
    if (clGetDeviceInfo(device, CL_DEVICE_NAME, required, device_name, NULL) == CL_SUCCESS) {
        snprintf(name, name_size, "%s", device_name);
    }
    free(device_name);
}

typedef struct opencl_state {
    cl_device_id device;
    cl_context context;
    cl_command_queue queue;
    cl_program program;
    cl_kernel kernel;
    int uses_amd_extension;
} opencl_state;

static opencl_state cached_opencl_state;
static pthread_mutex_t opencl_mutex = PTHREAD_MUTEX_INITIALIZER;

static int initialize_opencl(char *error_message, size_t error_message_size)
{
    if (cached_opencl_state.kernel != NULL)
        return 0;

    cl_int error = CL_SUCCESS;
    cached_opencl_state.device = find_fp64_gpu(&cached_opencl_state.uses_amd_extension);
    if (cached_opencl_state.device == NULL) {
        if (error_message != NULL && error_message_size > 0)
            snprintf(error_message, error_message_size,
                     "No OpenCL GPU with double precision support was found");
        return -1;
    }

    cached_opencl_state.context = clCreateContext(NULL, 1, &cached_opencl_state.device,
                                                   NULL, NULL, &error);
    if (cached_opencl_state.context == NULL || error != CL_SUCCESS)
        goto failed;
    cached_opencl_state.queue = clCreateCommandQueue(cached_opencl_state.context,
                                                      cached_opencl_state.device, 0, &error);
    if (cached_opencl_state.queue == NULL || error != CL_SUCCESS)
        goto failed;
    const char *source = fractal_opencl_kernel;
    size_t source_length = strlen(source);
    cached_opencl_state.program = clCreateProgramWithSource(cached_opencl_state.context,
        1, &source, &source_length, &error);
    if (cached_opencl_state.program == NULL || error != CL_SUCCESS)
        goto failed;

    if (clBuildProgram(cached_opencl_state.program, 1, &cached_opencl_state.device,
                       cached_opencl_state.uses_amd_extension ?
                       "-cl-std=CL1.2 -DUSE_AMD_FP64" : "-cl-std=CL1.2",
                       NULL, NULL) != CL_SUCCESS) {
        size_t log_size = 0;
        (void)clGetProgramBuildInfo(cached_opencl_state.program, cached_opencl_state.device,
                                    CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        if (error_message != NULL && error_message_size > 0 && log_size > 1) {
            size_t copy_size = log_size < error_message_size ? log_size : error_message_size - 1;
            (void)clGetProgramBuildInfo(cached_opencl_state.program, cached_opencl_state.device,
                                        CL_PROGRAM_BUILD_LOG, copy_size, error_message, NULL);
            error_message[copy_size] = '\0';
        }
        goto failed;
    }
    cached_opencl_state.kernel = clCreateKernel(cached_opencl_state.program,
                                                 "render_fractal", &error);
    if (cached_opencl_state.kernel == NULL || error != CL_SUCCESS)
        goto failed;
    return 0;

failed:
    if (error_message != NULL && error_message_size > 0 && error_message[0] == '\0')
        snprintf(error_message, error_message_size,
                 "OpenCL initialization failed (error %d)", error);
    if (cached_opencl_state.kernel != NULL) clReleaseKernel(cached_opencl_state.kernel);
    if (cached_opencl_state.program != NULL) clReleaseProgram(cached_opencl_state.program);
    if (cached_opencl_state.queue != NULL) clReleaseCommandQueue(cached_opencl_state.queue);
    if (cached_opencl_state.context != NULL) clReleaseContext(cached_opencl_state.context);
    memset(&cached_opencl_state, 0, sizeof(cached_opencl_state));
    return -1;
}

int fractal_opencl_available(char *device_name, size_t device_name_size)
{
    static int has_cached_device = -1;
    static char cached_name[128];
    if (has_cached_device < 0) {
        int uses_amd_extension = 0;
        cl_device_id device = find_fp64_gpu(&uses_amd_extension);
        has_cached_device = device != NULL;
        if (device != NULL)
            copy_device_name(device, cached_name, sizeof(cached_name));
    }
    if (!has_cached_device) {
        if (device_name != NULL && device_name_size > 0)
            snprintf(device_name, device_name_size, "No compatible OpenCL GPU with FP64 found");
        return 0;
    }
    if (device_name != NULL && device_name_size > 0)
        snprintf(device_name, device_name_size, "%s", cached_name);
    return 1;
}

int fractal_opencl_render(const fractal_config *config, uint8_t **rgba_out,
                          char *error_message, size_t error_message_size)
{
    int result = -1;
    cl_int error = CL_SUCCESS;
    cl_mem output_buffer = NULL;
    uint8_t *pixels = NULL;

    if (error_message != NULL && error_message_size > 0)
        error_message[0] = '\0';
    pthread_mutex_lock(&opencl_mutex);
    if (initialize_opencl(error_message, error_message_size) != 0) {
        pthread_mutex_unlock(&opencl_mutex);
        return -1;
    }

    const size_t pixel_count = config->width * config->height;
    const size_t byte_count = pixel_count * 4;
    const size_t global_size = (pixel_count + 63U) & ~(size_t)63U;
    cl_uint width = (cl_uint)config->width;
    cl_uint height = (cl_uint)config->height;
    cl_uint max_iterations = (cl_uint)fractal_effective_iterations(config);
    cl_uint kind = config->kind == FRACTAL_JULIA ? 1U : 0U;
    cl_uint color_mode = config->color_mode == FRACTAL_COLOR_AXIS_GRADIENT ? 1U : 0U;
    cl_ulong val_in = (cl_ulong)config->val_in;
    cl_ulong val_out = (cl_ulong)config->val_out;
    cl_uint attenuation = config->attenuation ? 1U : 0U;

    output_buffer = clCreateBuffer(cached_opencl_state.context, CL_MEM_WRITE_ONLY,
                                   byte_count, NULL, &error);
    if (output_buffer == NULL || error != CL_SUCCESS)
        goto opencl_error;

    double bounds[] = {config->x_min, config->x_max, config->y_min, config->y_max,
                       config->julia_re, config->julia_im};
    double colors_axis[] = {config->red_a, config->red_b, config->blue_a, config->blue_b};
    cl_uchar colors[] = {config->inside_color[0], config->inside_color[1], config->inside_color[2],
                         config->color_start[0], config->color_start[1], config->color_start[2],
                         config->color_end[0], config->color_end[1], config->color_end[2]};
#define SET_KERNEL_ARG(index, value) \
    do { error = clSetKernelArg(cached_opencl_state.kernel, index, sizeof(value), &(value)); \
         if (error != CL_SUCCESS) goto opencl_error; } while (0)
    SET_KERNEL_ARG(0, output_buffer);
    SET_KERNEL_ARG(1, width);
    SET_KERNEL_ARG(2, height);
    SET_KERNEL_ARG(3, max_iterations);
    SET_KERNEL_ARG(4, kind);
    for (cl_uint i = 0; i < 6; ++i) {
        error = clSetKernelArg(cached_opencl_state.kernel, 5 + i, sizeof(double), &bounds[i]);
        if (error != CL_SUCCESS)
            goto opencl_error;
    }
    error = clSetKernelArg(cached_opencl_state.kernel, 11, sizeof(config->escape_radius),
                           &config->escape_radius);
    if (error != CL_SUCCESS)
        goto opencl_error;
    SET_KERNEL_ARG(12, color_mode);
    for (cl_uint i = 0; i < 4; ++i) {
        error = clSetKernelArg(cached_opencl_state.kernel, 13 + i, sizeof(double),
                               &colors_axis[i]);
        if (error != CL_SUCCESS)
            goto opencl_error;
    }
    SET_KERNEL_ARG(17, val_in);
    SET_KERNEL_ARG(18, val_out);
    SET_KERNEL_ARG(19, attenuation);
    for (cl_uint i = 0; i < 9; ++i) {
        error = clSetKernelArg(cached_opencl_state.kernel, 20 + i, sizeof(cl_uchar), &colors[i]);
        if (error != CL_SUCCESS)
            goto opencl_error;
    }
#undef SET_KERNEL_ARG

    error = clEnqueueNDRangeKernel(cached_opencl_state.queue, cached_opencl_state.kernel,
                                   1, NULL, &global_size, NULL,
                                   0, NULL, NULL);
    if (error != CL_SUCCESS)
        goto opencl_error;
    error = clFinish(cached_opencl_state.queue);
    if (error != CL_SUCCESS)
        goto opencl_error;
    pixels = malloc(byte_count);
    if (pixels == NULL) {
        if (error_message != NULL && error_message_size > 0)
            snprintf(error_message, error_message_size, "Out of memory for GPU image readback");
        goto cleanup;
    }
    if (clEnqueueReadBuffer(cached_opencl_state.queue, output_buffer, CL_TRUE, 0, byte_count,
                            pixels, 0, NULL, NULL) != CL_SUCCESS)
        goto opencl_error;

    *rgba_out = pixels;
    pixels = NULL;
    result = 0;
    goto cleanup;

opencl_error:
    if (error_message != NULL && error_message_size > 0 && error_message[0] == '\0')
        snprintf(error_message, error_message_size, "OpenCL operation failed (error %d)", error);
cleanup:
    free(pixels);
    if (output_buffer != NULL) clReleaseMemObject(output_buffer);
    pthread_mutex_unlock(&opencl_mutex);
    return result;
}

void fractal_opencl_shutdown(void)
{
    pthread_mutex_lock(&opencl_mutex);
    if (cached_opencl_state.kernel != NULL) clReleaseKernel(cached_opencl_state.kernel);
    if (cached_opencl_state.program != NULL) clReleaseProgram(cached_opencl_state.program);
    if (cached_opencl_state.queue != NULL) clReleaseCommandQueue(cached_opencl_state.queue);
    if (cached_opencl_state.context != NULL) clReleaseContext(cached_opencl_state.context);
    memset(&cached_opencl_state, 0, sizeof(cached_opencl_state));
    pthread_mutex_unlock(&opencl_mutex);
}

#else

int fractal_opencl_available(char *device_name, size_t device_name_size)
{
    if (device_name != NULL && device_name_size > 0)
        snprintf(device_name, device_name_size, "OpenCL development files were not found at build time");
    return 0;
}

int fractal_opencl_render(const fractal_config *config, uint8_t **rgba_out,
                          char *error_message, size_t error_message_size)
{
    (void)config;
    (void)rgba_out;
    if (error_message != NULL && error_message_size > 0)
        snprintf(error_message, error_message_size,
                 "This build has no OpenCL support; install the OpenCL development files and rebuild");
    return -1;
}

void fractal_opencl_shutdown(void)
{
}

#endif
