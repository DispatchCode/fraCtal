#include <math.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>
#include <nuklear.h>
#include <nuklear_sdl_renderer.h>

#include "include/gui.h"
#include "include/mandelbrot.h"
#include "include/opencl_renderer.h"

#define PANEL_WIDTH 390
#define PREVIEW_MARGIN 18
#define SAVE_PATH_LENGTH 512
#define SECTION_FLAGS (NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_SCROLL_AUTO_HIDE)

typedef struct app_state {
    fractal_config config;
    SDL_mutex *mutex;
    SDL_Thread *worker;
    uint8_t *pixels;
    size_t image_width;
    size_t image_height;
    SDL_Texture *texture;
    SDL_Rect image_rect;
    char save_path[SAVE_PATH_LENGTH];
    char status[256];
    int rendering;
    int image_ready;
    int pending_render;
    int dragging_preview;
    int hand_cursor_selected;
    int last_mouse_x;
    int last_mouse_y;
    int drag_image_width;
    int drag_image_height;
    char color_hex[3][7];
} app_state;

typedef struct render_request {
    app_state *app;
    fractal_config config;
} render_request;

static int point_in_rect(int x, int y, const SDL_Rect *rect)
{
    return x >= rect->x && y >= rect->y &&
           x < rect->x + rect->w && y < rect->y + rect->h;
}

static size_t zoom_detail_scale(const fractal_config *config)
{
    if (!config->adaptive_resolution)
        return 1;
    double x_span = config->x_max - config->x_min;
    double y_span = config->y_max - config->y_min;
    if (!(x_span > 0.0) || !(y_span > 0.0))
        return 1;

    double zoom_x = 3.0 / x_span;
    double zoom_y = 2.0 / y_span;
    double zoom = zoom_x > zoom_y ? zoom_x : zoom_y;
    if (!(zoom > 1.0) || !isfinite(zoom))
        return 1;

    size_t scale = 1 + (size_t)(log2(zoom) / 4.0);
    return scale > 4 ? 4 : scale;
}

static int render_worker(void *opaque)
{
    render_request *request = opaque;
    app_state *app = request->app;
    uint8_t *new_pixels = NULL;
    int result = fractal_render(&request->config, &new_pixels);

    SDL_LockMutex(app->mutex);
    if (result == 0) {
        free(app->pixels);
        app->pixels = new_pixels;
        app->image_width = request->config.width;
        app->image_height = request->config.height;
        app->image_ready = 1;
        snprintf(app->status, sizeof(app->status), "Rendered %zu x %zu on %s, %zu iterations",
                 app->image_width, app->image_height,
                 request->config.backend == FRACTAL_BACKEND_OPENCL ? "OpenCL GPU" : "CPU",
                 fractal_effective_iterations(&request->config));
    } else {
        free(new_pixels);
        snprintf(app->status, sizeof(app->status), "Render failed on %s: check settings, memory, and GPU support",
                 request->config.backend == FRACTAL_BACKEND_OPENCL ? "OpenCL GPU" : "CPU");
    }
    app->rendering = 0;
    SDL_UnlockMutex(app->mutex);

    free(request);
    return result;
}

static void start_render(app_state *app)
{
    if (app->worker != NULL || !app->pending_render)
        return;

    app->pending_render = 0;
    render_request *request = malloc(sizeof(*request));
    if (request == NULL) {
        snprintf(app->status, sizeof(app->status), "Render failed: out of memory");
        return;
    }

    request->app = app;
    request->config = app->config;
    size_t scale = zoom_detail_scale(&request->config);
    if (request->config.width > 8192 / scale)
        scale = 8192 / request->config.width;
    if (request->config.height > 8192 / scale)
        scale = 8192 / request->config.height;
    if (scale < 1)
        scale = 1;
    request->config.width *= scale;
    request->config.height *= scale;

    SDL_LockMutex(app->mutex);
    app->rendering = 1;
    snprintf(app->status, sizeof(app->status), "Rendering...");
    SDL_UnlockMutex(app->mutex);

    app->worker = SDL_CreateThread(render_worker, "fractal-render", request);
    if (app->worker == NULL) {
        SDL_LockMutex(app->mutex);
        app->rendering = 0;
        snprintf(app->status, sizeof(app->status), "Could not start render thread: %s", SDL_GetError());
        SDL_UnlockMutex(app->mutex);
        free(request);
    }
}

static void finish_render(app_state *app, SDL_Renderer *renderer)
{
    if (app->worker == NULL)
        return;

    SDL_LockMutex(app->mutex);
    int finished = !app->rendering;
    SDL_UnlockMutex(app->mutex);
    if (!finished)
        return;

    SDL_WaitThread(app->worker, NULL);
    app->worker = NULL;

    SDL_LockMutex(app->mutex);
    if (app->image_ready) {
        if (app->texture != NULL)
            SDL_DestroyTexture(app->texture);
        app->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                         SDL_TEXTUREACCESS_STATIC,
                                         (int)app->image_width,
                                         (int)app->image_height);
        if (app->texture != NULL) {
            SDL_UpdateTexture(app->texture, NULL, app->pixels,
                              (int)(app->image_width * 4));
            SDL_SetTextureBlendMode(app->texture, SDL_BLENDMODE_NONE);
        } else {
            snprintf(app->status, sizeof(app->status), "Texture creation failed: %s", SDL_GetError());
        }
        app->image_ready = 0;
    }
    SDL_UnlockMutex(app->mutex);
}

static void property_int(struct nk_context *ctx, const char *name,
                         int *value, int minimum, int maximum,
                         int step, int increment_per_pixel)
{
    nk_layout_row_dynamic(ctx, 26.0f, 1);
    nk_property_int(ctx, name, minimum, value, maximum, step, increment_per_pixel);
}

static void property_double(struct nk_context *ctx, const char *name,
                            double *value, double minimum, double maximum,
                            double step)
{
    nk_layout_row_dynamic(ctx, 26.0f, 1);
    nk_property_double(ctx, name, minimum, value, maximum, step,
                       (float)(step * 10.0));
}

static void format_color_hex(const uint8_t color[3], char hex[7])
{
    snprintf(hex, 7, "%02X%02X%02X", color[0], color[1], color[2]);
}

static int parse_color_hex(const char hex[7], uint8_t color[3])
{
    if (strlen(hex) != 6)
        return 0;
    for (size_t i = 0; i < 6; ++i) {
        if (!isxdigit((unsigned char)hex[i]))
            return 0;
    }
    for (size_t channel = 0; channel < 3; ++channel) {
        unsigned int value = 0;
        if (sscanf(hex + channel * 2, "%2x", &value) != 1)
            return 0;
        color[channel] = (uint8_t)value;
    }
    return 1;
}

static void color_hex_control(struct nk_context *ctx, const char *title,
                              char hex[7], uint8_t color[3])
{
    nk_layout_row_dynamic(ctx, 22.0f, 1);
    nk_label(ctx, title, NK_TEXT_LEFT);
    nk_layout_row_dynamic(ctx, 28.0f, 2);
    struct nk_color swatch = nk_rgb(color[0], color[1], color[2]);
    if (nk_combo_begin_color(ctx, swatch, nk_vec2(260.0f, 300.0f))) {
        nk_layout_row_dynamic(ctx, 150.0f, 1);
        struct nk_colorf selected = nk_color_cf(swatch);
        selected = nk_color_picker(ctx, selected, NK_RGB);
        struct nk_color picked = nk_rgb_cf(selected);
        if (picked.r != color[0] || picked.g != color[1] || picked.b != color[2]) {
            color[0] = picked.r;
            color[1] = picked.g;
            color[2] = picked.b;
            format_color_hex(color, hex);
        }
        nk_combo_end(ctx);
    }
    nk_label(ctx, "Click swatch to pick", NK_TEXT_LEFT);
    nk_layout_row_dynamic(ctx, 27.0f, 2);
    nk_label(ctx, "Hex #", NK_TEXT_LEFT);
    nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, hex, 7, nk_filter_hex);
    (void)parse_color_hex(hex, color);
    nk_layout_row_dynamic(ctx, 18.0f, 1);
    if (strlen(hex) != 6)
        nk_label_colored(ctx, "Enter six hexadecimal digits", NK_TEXT_LEFT,
                         nk_rgb(235, 170, 100));
    else
        nk_label(ctx, "Example: FF8040", NK_TEXT_LEFT);
}

static void color_range_controls(struct nk_context *ctx, fractal_config *config)
{
    property_double(ctx, "Red at left (red_a)", &config->red_a, 0.0, 255.0, 1.0);
    property_double(ctx, "Red at right (red_b)", &config->red_b, 0.0, 255.0, 1.0);
    property_double(ctx, "Blue at top (blu_a)", &config->blue_a, 0.0, 255.0, 1.0);
    property_double(ctx, "Blue at bottom (blu_b)", &config->blue_b, 0.0, 255.0, 1.0);
    int val_in = (int)config->val_in;
    int val_out = (int)config->val_out;
    property_int(ctx, "Green threshold (val_in)", &val_in, 0, 1000000, 1, 1);
    property_int(ctx, "Green offset (val_out)", &val_out, 0, 255, 1, 1);
    config->val_in = (size_t)val_in;
    config->val_out = (size_t)val_out;
    nk_layout_row_dynamic(ctx, 24.0f, 1);
    nk_checkbox_label(ctx, "Attenuate red and blue", &config->attenuation);
}

static void apply_app_style(struct nk_context *ctx)
{
    struct nk_color palette[NK_COLOR_COUNT];
    palette[NK_COLOR_TEXT] = nk_rgb(229, 234, 243);
    palette[NK_COLOR_WINDOW] = nk_rgb(17, 22, 32);
    palette[NK_COLOR_HEADER] = nk_rgb(31, 40, 55);
    palette[NK_COLOR_BORDER] = nk_rgb(48, 60, 79);
    palette[NK_COLOR_BUTTON] = nk_rgb(39, 50, 68);
    palette[NK_COLOR_BUTTON_HOVER] = nk_rgb(52, 68, 91);
    palette[NK_COLOR_BUTTON_ACTIVE] = nk_rgb(62, 91, 132);
    palette[NK_COLOR_TOGGLE] = nk_rgb(39, 50, 68);
    palette[NK_COLOR_TOGGLE_HOVER] = nk_rgb(52, 68, 91);
    palette[NK_COLOR_TOGGLE_CURSOR] = nk_rgb(111, 168, 254);
    palette[NK_COLOR_SELECT] = nk_rgb(35, 45, 61);
    palette[NK_COLOR_SELECT_ACTIVE] = nk_rgb(48, 69, 98);
    palette[NK_COLOR_SLIDER] = nk_rgb(35, 45, 61);
    palette[NK_COLOR_SLIDER_CURSOR] = nk_rgb(83, 130, 197);
    palette[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgb(103, 153, 224);
    palette[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgb(130, 174, 235);
    palette[NK_COLOR_PROPERTY] = nk_rgb(28, 36, 49);
    palette[NK_COLOR_EDIT] = nk_rgb(24, 31, 43);
    palette[NK_COLOR_EDIT_CURSOR] = nk_rgb(229, 234, 243);
    palette[NK_COLOR_COMBO] = nk_rgb(28, 36, 49);
    palette[NK_COLOR_CHART] = nk_rgb(24, 31, 43);
    palette[NK_COLOR_CHART_COLOR] = nk_rgb(111, 168, 254);
    palette[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgb(147, 197, 253);
    palette[NK_COLOR_SCROLLBAR] = nk_rgb(20, 26, 37);
    palette[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgb(54, 67, 88);
    palette[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgb(72, 91, 120);
    palette[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgb(91, 116, 153);
    palette[NK_COLOR_TAB_HEADER] = nk_rgb(27, 35, 49);
    palette[NK_COLOR_KNOB] = palette[NK_COLOR_SLIDER];
    palette[NK_COLOR_KNOB_CURSOR] = palette[NK_COLOR_SLIDER_CURSOR];
    palette[NK_COLOR_KNOB_CURSOR_HOVER] = palette[NK_COLOR_SLIDER_CURSOR_HOVER];
    palette[NK_COLOR_KNOB_CURSOR_ACTIVE] = palette[NK_COLOR_SLIDER_CURSOR_ACTIVE];
    nk_style_from_table(ctx, palette);
    ctx->style.window.rounding = 7.0f;
    ctx->style.window.group_border = 1.0f;
    ctx->style.window.group_padding = nk_vec2(9.0f, 8.0f);
    ctx->style.window.spacing = nk_vec2(8.0f, 7.0f);
    ctx->style.window.padding = nk_vec2(11.0f, 10.0f);
    ctx->style.button.rounding = 5.0f;
    ctx->style.button.border = 0.0f;
}

static void draw_controls(app_state *app, struct nk_context *ctx,
                          int window_width, int window_height)
{
    struct nk_rect bounds = nk_rect(10.0f, 10.0f, (float)PANEL_WIDTH,
                                    (float)(window_height - 20));
    nk_flags flags = NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_SCROLL_AUTO_HIDE;
    if (!nk_begin(ctx, "Fractal settings", bounds, flags)) {
        nk_end(ctx);
        return;
    }

    int available_cpus = SDL_GetCPUCount();
    if (available_cpus < 1)
        available_cpus = 1;
    if (available_cpus > 256)
        available_cpus = 256;

    nk_layout_row_dynamic(ctx, 132.0f, 1);
    if (nk_group_begin(ctx, "Renderer selected for next render", SECTION_FLAGS)) {
        char gpu_name[128];
        int opencl_available = fractal_backend_available(FRACTAL_BACKEND_OPENCL,
                                                           gpu_name, sizeof(gpu_name));
        nk_layout_row_dynamic(ctx, 26.0f, 2);
        if (nk_option_label(ctx, "CPU", app->config.backend == FRACTAL_BACKEND_CPU))
            app->config.backend = FRACTAL_BACKEND_CPU;
        if (opencl_available) {
            if (nk_option_label(ctx, "OpenCL GPU", app->config.backend == FRACTAL_BACKEND_OPENCL))
                app->config.backend = FRACTAL_BACKEND_OPENCL;
        } else {
            nk_label(ctx, "OpenCL GPU unavailable", NK_TEXT_LEFT);
        }

        char backend_status[192];
        struct nk_color backend_color;
        if (app->config.backend == FRACTAL_BACKEND_CPU) {
            snprintf(backend_status, sizeof(backend_status), "CPU  |  %zu threads",
                     app->config.thread_count);
            backend_color = nk_rgb(105, 205, 255);
        } else if (app->config.backend == FRACTAL_BACKEND_OPENCL) {
            if (opencl_available)
                snprintf(backend_status, sizeof(backend_status), "GPU / OpenCL selected");
            else
                snprintf(backend_status, sizeof(backend_status), "GPU / OpenCL  |  device unavailable");
            backend_color = nk_rgb(130, 235, 130);
        } else {
            snprintf(backend_status, sizeof(backend_status), "Renderer unavailable");
            backend_color = nk_rgb(220, 175, 105);
        }
        nk_layout_row_dynamic(ctx, 26.0f, 1);
        nk_label_colored(ctx, backend_status, NK_TEXT_LEFT, backend_color);
        if (app->config.backend == FRACTAL_BACKEND_OPENCL && opencl_available) {
            nk_layout_row_dynamic(ctx, 19.0f, 1);
            nk_label(ctx, gpu_name, NK_TEXT_LEFT);
        }
        nk_group_end(ctx);
    }

    nk_layout_row_dynamic(ctx, 285.0f, 1);
    if (nk_group_begin(ctx, "Fractal and quality", SECTION_FLAGS)) {
        nk_layout_row_dynamic(ctx, 24.0f, 2);
        if (nk_option_label(ctx, "Mandelbrot", app->config.kind == FRACTAL_MANDELBROT))
            app->config.kind = FRACTAL_MANDELBROT;
        if (nk_option_label(ctx, "Julia", app->config.kind == FRACTAL_JULIA))
            app->config.kind = FRACTAL_JULIA;

        int width = (int)app->config.width;
        int height = (int)app->config.height;
        int iterations = (int)app->config.max_iterations;
        int threads = (int)app->config.thread_count;
        property_int(ctx, "Base width", &width, 64, 8192, 64, 1);
        property_int(ctx, "Base height", &height, 64, 8192, 64, 1);
        property_int(ctx, "Base iterations", &iterations, 1, 1000000, 100, 1);
        property_double(ctx, "Escape threshold (|z| squared)", &app->config.escape_radius,
                        0.0001, 1.0e12, 0.1);
        char thread_label[64];
        snprintf(thread_label, sizeof(thread_label), "CPU threads (%d available)", available_cpus);
        property_int(ctx, thread_label, &threads, 1, available_cpus, 1, 1);
        app->config.width = (size_t)width;
        app->config.height = (size_t)height;
        app->config.max_iterations = (size_t)iterations;
        app->config.thread_count = (size_t)threads;

        nk_layout_row_dynamic(ctx, 24.0f, 1);
        nk_checkbox_label(ctx, "Increase iterations with zoom", &app->config.adaptive_iterations);
        nk_layout_row_dynamic(ctx, 24.0f, 1);
        nk_checkbox_label(ctx, "Increase resolution with zoom", &app->config.adaptive_resolution);
        nk_layout_row_dynamic(ctx, 25.0f, 1);
        if (nk_button_label(ctx, "Use all available CPUs"))
            app->config.thread_count = (size_t)available_cpus;
        nk_group_end(ctx);
    }

    nk_layout_row_dynamic(ctx, 248.0f, 1);
    if (nk_group_begin(ctx, "View coordinates", SECTION_FLAGS)) {
        nk_layout_row_dynamic(ctx, 24.0f, 1);
        nk_label(ctx, "Complex plane bounds", NK_TEXT_LEFT);
        property_double(ctx, "Real minimum (left)", &app->config.x_min, -1.0e6, 1.0e6, 0.0001);
        property_double(ctx, "Real maximum (right)", &app->config.x_max, -1.0e6, 1.0e6, 0.0001);
        property_double(ctx, "Imaginary minimum (bottom)", &app->config.y_min, -1.0e6, 1.0e6, 0.0001);
        property_double(ctx, "Imaginary maximum (top)", &app->config.y_max, -1.0e6, 1.0e6, 0.0001);

        if (app->config.kind == FRACTAL_JULIA) {
            nk_layout_row_dynamic(ctx, 24.0f, 1);
            nk_label(ctx, "Julia constant", NK_TEXT_LEFT);
            property_double(ctx, "c real", &app->config.julia_re, -2.0, 2.0, 0.001);
            property_double(ctx, "c imaginary", &app->config.julia_im, -2.0, 2.0, 0.001);
        }
        nk_group_end(ctx);
    }

    nk_layout_row_dynamic(ctx, 425.0f, 1);
    if (nk_group_begin(ctx, "Color and gradient", SECTION_FLAGS)) {
        nk_layout_row_dynamic(ctx, 22.0f, 1);
        nk_label(ctx, "Color algorithm", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 24.0f, 2);
        fractal_color_mode previous_color_mode = app->config.color_mode;
        if (nk_option_label(ctx, "Iteration gradient",
                            app->config.color_mode == FRACTAL_COLOR_ITERATION_GRADIENT))
            app->config.color_mode = FRACTAL_COLOR_ITERATION_GRADIENT;
        if (nk_option_label(ctx, "Coordinate RGB",
                            app->config.color_mode == FRACTAL_COLOR_AXIS_GRADIENT))
            app->config.color_mode = FRACTAL_COLOR_AXIS_GRADIENT;
        if (app->config.color_mode != previous_color_mode)
            app->pending_render = 1;

        if (app->config.color_mode == FRACTAL_COLOR_ITERATION_GRADIENT) {
            color_hex_control(ctx, "Interior color", app->color_hex[0], app->config.inside_color);
            color_hex_control(ctx, "Gradient start", app->color_hex[1], app->config.color_start);
            color_hex_control(ctx, "Gradient end", app->color_hex[2], app->config.color_end);
        } else {
            nk_layout_row_dynamic(ctx, 34.0f, 1);
            nk_label_wrap(ctx, "Interior colors are derived from the iteration count and attenuation in this mode.");
            color_range_controls(ctx, &app->config);
        }
        nk_group_end(ctx);
    }

    nk_layout_row_dynamic(ctx, 225.0f, 1);
    if (nk_group_begin(ctx, "Render and export", SECTION_FLAGS)) {
        nk_layout_row_dynamic(ctx, 28.0f, 2);
        if (nk_button_label(ctx, "Render"))
            app->pending_render = 1;
        if (nk_button_label(ctx, "Reset view")) {
            app->config.x_min = -2.0;
            app->config.x_max = 1.0;
            app->config.y_min = -1.0;
            app->config.y_max = 1.0;
            app->pending_render = 1;
        }

        nk_layout_row_dynamic(ctx, 22.0f, 1);
        nk_label(ctx, "PNG output path", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 28.0f, 1);
        nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, app->save_path,
                                       (int)sizeof(app->save_path), nk_filter_default);
        nk_layout_row_dynamic(ctx, 28.0f, 1);
        if (nk_button_label(ctx, "Save PNG")) {
            SDL_LockMutex(app->mutex);
            if (app->pixels == NULL) {
                snprintf(app->status, sizeof(app->status), "Render an image before saving");
            } else {
                unsigned error = fractal_save_png(app->save_path, app->pixels,
                                                  app->image_width, app->image_height);
                if (error == 0)
                    snprintf(app->status, sizeof(app->status), "Saved %.245s", app->save_path);
                else
                    snprintf(app->status, sizeof(app->status), "PNG save error %u", error);
            }
            SDL_UnlockMutex(app->mutex);
        }

        nk_layout_row_dynamic(ctx, 38.0f, 1);
        char status[sizeof(app->status)];
        SDL_LockMutex(app->mutex);
        memcpy(status, app->status, sizeof(status));
        SDL_UnlockMutex(app->mutex);
        status[sizeof(status) - 1] = '\0';
        nk_label_wrap(ctx, status);
        nk_layout_row_dynamic(ctx, 34.0f, 1);
        nk_label_wrap(ctx, "Mouse wheel: zoom. Drag inside preview: pan. Zoom can raise output resolution and iterations automatically.");
        nk_group_end(ctx);
    }

    nk_end(ctx);
    (void)window_width;
}

static SDL_Rect calculate_image_rect(app_state *app, int window_width, int window_height)
{
    SDL_Rect result = {
        PANEL_WIDTH + PREVIEW_MARGIN,
        PREVIEW_MARGIN,
        window_width - PANEL_WIDTH - PREVIEW_MARGIN * 2,
        window_height - PREVIEW_MARGIN * 2
    };
    if (app->texture == NULL || result.w <= 0 || result.h <= 0)
        return result;

    double scale_x = (double)result.w / (double)app->image_width;
    double scale_y = (double)result.h / (double)app->image_height;
    double scale = scale_x < scale_y ? scale_x : scale_y;
    int image_width = (int)((double)app->image_width * scale);
    int image_height = (int)((double)app->image_height * scale);
    result.x += (result.w - image_width) / 2;
    result.y += (result.h - image_height) / 2;
    result.w = image_width;
    result.h = image_height;
    return result;
}

static void zoom_at_cursor(app_state *app, int mouse_x, int mouse_y, double wheel_delta)
{
    const SDL_Rect *rect = &app->image_rect;
    if (rect->w <= 0 || rect->h <= 0 || wheel_delta == 0.0)
        return;

    double u = (double)(mouse_x - rect->x) / (double)rect->w;
    double v = (double)(mouse_y - rect->y) / (double)rect->h;
    double old_x_span = app->config.x_max - app->config.x_min;
    double old_y_span = app->config.y_max - app->config.y_min;
    double center_x = app->config.x_min + u * old_x_span;
    double center_y = app->config.y_max - v * old_y_span;
    double factor = pow(0.8, wheel_delta);
    double new_x_span = old_x_span * factor;
    double new_y_span = old_y_span * factor;

    app->config.x_min = center_x - u * new_x_span;
    app->config.x_max = app->config.x_min + new_x_span;
    app->config.y_max = center_y + v * new_y_span;
    app->config.y_min = app->config.y_max - new_y_span;
    app->pending_render = 1;
}

static void handle_preview_event(app_state *app, const SDL_Event *event)
{
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT &&
        point_in_rect(event->button.x, event->button.y, &app->image_rect)) {
        app->dragging_preview = 1;
        app->last_mouse_x = event->button.x;
        app->last_mouse_y = event->button.y;
        app->drag_image_width = app->image_rect.w;
        app->drag_image_height = app->image_rect.h;
        SDL_CaptureMouse(SDL_TRUE);
    } else if (event->type == SDL_MOUSEMOTION && app->dragging_preview) {
        if (app->drag_image_width <= 0 || app->drag_image_height <= 0)
            return;
        double x_span = app->config.x_max - app->config.x_min;
        double y_span = app->config.y_max - app->config.y_min;
        int delta_x = event->motion.x - app->last_mouse_x;
        int delta_y = event->motion.y - app->last_mouse_y;
        app->config.x_min -= (double)delta_x * x_span / (double)app->drag_image_width;
        app->config.x_max -= (double)delta_x * x_span / (double)app->drag_image_width;
        app->config.y_min += (double)delta_y * y_span / (double)app->drag_image_height;
        app->config.y_max += (double)delta_y * y_span / (double)app->drag_image_height;
        app->last_mouse_x = event->motion.x;
        app->last_mouse_y = event->motion.y;
    } else if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT &&
               app->dragging_preview) {
        app->dragging_preview = 0;
        SDL_CaptureMouse(SDL_FALSE);
        app->pending_render = 1;
    } else if (event->type == SDL_WINDOWEVENT &&
               event->window.event == SDL_WINDOWEVENT_FOCUS_LOST && app->dragging_preview) {
        app->dragging_preview = 0;
        SDL_CaptureMouse(SDL_FALSE);
        app->pending_render = 1;
    } else if (event->type == SDL_MOUSEWHEEL) {
        int mouse_x, mouse_y;
#if SDL_VERSION_ATLEAST(2, 26, 0)
        mouse_x = event->wheel.mouseX;
        mouse_y = event->wheel.mouseY;
#else
        SDL_GetMouseState(&mouse_x, &mouse_y);
#endif
        if (!point_in_rect(mouse_x, mouse_y, &app->image_rect))
            SDL_GetMouseState(&mouse_x, &mouse_y);
        double wheel_delta;
#if SDL_VERSION_ATLEAST(2, 0, 18)
        wheel_delta = (double)event->wheel.preciseY;
        if (wheel_delta == 0.0)
            wheel_delta = (double)event->wheel.y;
#else
        wheel_delta = (double)event->wheel.y;
#endif
        if (event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
            wheel_delta = -wheel_delta;
        if (point_in_rect(mouse_x, mouse_y, &app->image_rect))
            zoom_at_cursor(app, mouse_x, mouse_y, wheel_delta);
    }
}

static void draw_preview(app_state *app, SDL_Renderer *renderer,
                         int window_width, int window_height)
{
    SDL_SetRenderDrawColor(renderer, 10, 14, 21, 255);
    SDL_RenderClear(renderer);

    SDL_Rect area = {
        PANEL_WIDTH + PREVIEW_MARGIN,
        PREVIEW_MARGIN,
        window_width - PANEL_WIDTH - PREVIEW_MARGIN * 2,
        window_height - PREVIEW_MARGIN * 2
    };
    SDL_SetRenderDrawColor(renderer, 18, 25, 37, 255);
    SDL_RenderFillRect(renderer, &area);
    SDL_SetRenderDrawColor(renderer, 46, 59, 79, 255);
    SDL_RenderDrawRect(renderer, &area);

    app->image_rect = calculate_image_rect(app, window_width, window_height);
    if (app->texture != NULL) {
        SDL_RenderCopy(renderer, app->texture, NULL, &app->image_rect);
        SDL_SetRenderDrawColor(renderer, 87, 116, 158, 255);
        SDL_RenderDrawRect(renderer, &app->image_rect);
    }
}

static void update_preview_cursor(app_state *app, SDL_Cursor *hand_cursor)
{
    if (hand_cursor == NULL)
        return;

    int mouse_x, mouse_y;
    SDL_GetMouseState(&mouse_x, &mouse_y);
    int over_image = app->texture != NULL &&
                     point_in_rect(mouse_x, mouse_y, &app->image_rect);
    if (over_image == app->hand_cursor_selected)
        return;

    SDL_SetCursor(over_image ? hand_cursor : SDL_GetDefaultCursor());
    app->hand_cursor_selected = over_image;
}

int fractal_gui_run(void)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("fraCtal", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, 1440, 900,
                                          SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (window == NULL) {
        fprintf(stderr, "SDL window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
                                                 SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL)
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (renderer == NULL) {
        fprintf(stderr, "SDL renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    struct nk_context *ctx = nk_sdl_init(window, renderer);
    if (ctx == NULL) {
        fprintf(stderr, "Nuklear initialization failed\n");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    apply_app_style(ctx);

    struct nk_font_atlas *atlas;
    nk_sdl_font_stash_begin(&atlas);
    struct nk_font *font = nk_font_atlas_add_default(atlas, 16.0f, NULL);
    nk_sdl_font_stash_end();
    if (font == NULL) {
        fprintf(stderr, "Nuklear default font creation failed\n");
        nk_sdl_shutdown();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    nk_style_set_font(ctx, &font->handle);
    SDL_StartTextInput();

    app_state app = {0};
    fractal_config_defaults(&app.config);
    format_color_hex(app.config.inside_color, app.color_hex[0]);
    format_color_hex(app.config.color_start, app.color_hex[1]);
    format_color_hex(app.config.color_end, app.color_hex[2]);
    int available_cpus = SDL_GetCPUCount();
    if (available_cpus > 0 && available_cpus <= 256)
        app.config.thread_count = (size_t)available_cpus;
    else if (available_cpus > 256)
        app.config.thread_count = 256;
    app.mutex = SDL_CreateMutex();
    snprintf(app.save_path, sizeof(app.save_path), "mandelbrot.png");
    snprintf(app.status, sizeof(app.status), "Ready");
    app.pending_render = 1;
    if (app.mutex == NULL) {
        fprintf(stderr, "Could not create render mutex: %s\n", SDL_GetError());
        nk_sdl_shutdown();
        SDL_StopTextInput();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_Cursor *hand_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);

    int running = 1;
    while (running) {
        finish_render(&app, renderer);
        start_render(&app);

        nk_input_begin(ctx);
        int window_width, window_height;
        SDL_GetWindowSize(window, &window_width, &window_height);
        app.image_rect = calculate_image_rect(&app, window_width, window_height);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            nk_sdl_handle_event(&event);
            handle_preview_event(&app, &event);
            if (event.type == SDL_QUIT)
                running = 0;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
                running = 0;
        }
        nk_input_end(ctx);

        draw_preview(&app, renderer, window_width, window_height);
        update_preview_cursor(&app, hand_cursor);
        draw_controls(&app, ctx, window_width, window_height);
        nk_sdl_render(NK_ANTI_ALIASING_ON);
        SDL_RenderPresent(renderer);
    }

    if (app.worker != NULL)
        SDL_WaitThread(app.worker, NULL);
    if (app.texture != NULL)
        SDL_DestroyTexture(app.texture);
    free(app.pixels);
    SDL_DestroyMutex(app.mutex);
    if (hand_cursor != NULL)
        SDL_FreeCursor(hand_cursor);
    fractal_opencl_shutdown();
    nk_sdl_shutdown();
    SDL_StopTextInput();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
