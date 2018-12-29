#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "include/mandelbrot.h"
#include "include/complex.h"
#include "lib/ppmimage/include/ppm.h"


#define   X0   (2.0)
#define   Y0   (1.7)
#define   X1   (-2.0)
#define   Y1   (-1.7)


#define   DIFF_Y (Y0-Y1)
#define   DIFF_X (X0-X1)


char            *pixels;
static uint32_t *palette;
static size_t    palette_size;


_inline uint8_t get_channel(uint32_t color, size_t bits)
{
    return (color >> bits) & 0xFF;
}

_inline int linear_index(int x, int y, int w)
{
    return 3 * (x + y*w);
}

/*
 * Load a simple text file with the color palette in the following format:
 *
 * length
 * r,g,b
 * r,g,b
 *
 * and so on
 *
 */
static void load_palette(char* palette_file)
{
    FILE *file = fopen(palette_file, "r");

    char line[16];

    if(file == NULL)
        printf("\npalette file not found\n");

    fgets(line,16,file);

    uint8_t size = (uint8_t) atoi(line);
    palette = calloc(size, sizeof(uint32_t));

    palette_size = size;
    int index = 0;

    while(fgets(line,16,file))
    {
        int32_t color = 0;
        uint8_t pos = 16;
        char *token = strtok(line, ",\r\n");

        do {
            color |= (atoi(token) << pos);
            token = strtok(NULL, ",\r\n");
            pos -= 8;
        }while(token != NULL);

        palette[index++] = 0xFF000000 | color;
    }

    fclose(file);
}

/*
 * The hearth of the algorithm: is the entry point of each
 * thread that begins execution
 *
 */
void *calc(_square_coords *coords)
{
    for(int i = coords->start_y; i < coords->end_y; i++)
    {
        double scaled_y = Y1 + (((double)i * DIFF_Y) / (double)coords->h);
        for(int j = coords->start_x; j<coords->end_x; j++)
        {
            double scaled_x = X1 + (((double)j * DIFF_X) / (double)coords->w);

            complex z, c;
            z.re = scaled_x;
            z.im = scaled_y;

            c.re = scaled_x;
            c.im = scaled_y;

            int k;
            double abs=0.0;
            for(k = 0; k < coords->max_iterations; k++)
            {
                z   = complex_sum(complex_mul(z, z), c);
                abs = complex_abs(z);

                if(abs > 4.0) {
                    break;
                }
            }

            int index = linear_index(j, i, coords->w);

            if(abs <= 4.0 )
            {
                pixels[index]   = 0;
                pixels[index+1] = 0;
                pixels[index+2] = 0;
            }
            else
            {

#ifndef PALETTE // refer to README
                int r =  k >> 2;
                int g =  k >> 1;
                int b =  k << 3;

                r = (r > 128) ? 128 : r;
                g = (g > 128) ? 128 : g;
                b = (b > 255) ? 255 : b;

                pixels[index    ] = r;
                pixels[index + 1] = g;
                pixels[index + 2] = b;
#else
                uint32_t color = palette[k % palette_size];

                pixels[index    ] = get_channel(color,16);
                pixels[index + 1] = get_channel(color,8);
                pixels[index + 2] = get_channel(color,0);
#endif
            }
        }
    }
}

/*
 * Ugly code that works. It splits the image in n_threads square;
 * each square is a window for a thread
 *
 */
void init(size_t w, size_t h, size_t n_threads,size_t iterations, mandelbrot_info *info)
{
    size_t middle_x = w >> 1;
    size_t middle_y = h >> 1;

    info->h = h;
    info->w = w;
    info->n_quadrants = n_threads;
    info->coords      = calloc(n_threads, sizeof(_square_coords));

    int m_th = n_threads >> 1;
    for(int i=0; i < n_threads; i++)
    {
        if(i < m_th)
        {
            info->coords[i].start_x = 0;
            info->coords[i].end_x = middle_x;
        }
        else
        {
            info->coords[i].start_x = middle_x;
            info->coords[i].end_x = w;
        }

        if(i&1)
        {
            info->coords[i].start_y = middle_y;
            info->coords[i].end_y = h;
        }
        else
        {
            info->coords[i].start_y = 0;
            info->coords[i].end_y = middle_y;
        }

        info->coords[i].w = w;
        info->coords[i].h = h;
        info->coords[i].max_iterations = iterations;
    }

#ifdef PALETTE
   load_palette("palette.txt");
#endif
}

/*
 * Run threads and wait until they ended
 *
 */
void generation(mandelbrot_info mandelbrot, char *filename)
{
    pixels = (char*) calloc(3 + (mandelbrot.w * mandelbrot.h * 3), 8);

    pthread_t square[mandelbrot.n_quadrants];

    for(int i=0; i<mandelbrot.n_quadrants; i++)
        pthread_create(&square[i], NULL, (void*) calc, (void*)&mandelbrot.coords[i]);

    for(int i=0; i<mandelbrot.n_quadrants; i++)
        pthread_join(square[i], NULL);

    free(mandelbrot.coords);

    save_image(filename, pixels, mandelbrot.w, mandelbrot.h);
    free(pixels);
}