#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/fractal.h"
#include "include/complex.h"


/*
    compilation: gcc fractal.c lib/ppmimage/ppm.c complex.c  -o fractal
*/

void init_array(fractal_info *info) 
{
    info->x0 = 1.5;
    info->y0 = 1.5;
    info->x1 = -2.0;
    info->y1 = -1.5;
    
    fractal.pixels = (char*) calloc(3 + (info->w * info->h * 3), 8);
}

int depth_calculation(fractal_info info, double scaled_x, double scaled_y, double *mod)
{
    complex z;
    z.re = scaled_x;
    z.im = scaled_y;
    
    complex zn = z;

    int i;
    for(i = 0; i < info.depth; i++)
    {
        z    = complex_sum(complex_mul(z, z), zn);
        *mod = complex_abs(z);

        if(*mod > 4.0) {
            break;
        }
    }

    return i;
}

void generate_mandelbrot(fractal_info info) 
{
    int i, j;
    
    for(j = 0; j < info.h; j++)
    {
        double y = info.y1 + (((double)j * (info.y0-info.y1)) / (double)info.h);

        for(i = 0; i < info.w; i++)
        {
            double x = info.x1 + (((double)i * (info.x0-info.x1)) / (double)info.w);
            
            double modulo;
            int iteration = depth_calculation(info, x, y, &modulo);

            if(modulo <= 4.0) 
            {
                fractal.pixels[INDEX(i, j, info.w)    ] = 0;
                fractal.pixels[INDEX(i, j, info.w) + 1] = 0;
                fractal.pixels[INDEX(i, j, info.w) + 2] = 0;
            }
            else
            {
                int r =  iteration>>2;
                int g =  (int)iteration<<1;
                int b =  (int)iteration + (iteration<<4);
                
                r = (r>200) ? 200 : r;
                g = (g>200) ? 200 : g;
                b = (b>255) ? 255 : b;
                
                fractal.pixels[INDEX(i, j, info.w)    ] = r;
                fractal.pixels[INDEX(i, j, info.w) + 1] = g;
                fractal.pixels[INDEX(i, j, info.w) + 2] = b;
            }
        }
    }
      
    initialize(info.w,info.h);
    set_pixel_array(fractal.pixels);
    save_image(info.name);
}


void generate_julia(fractal_info info) {}


void generate(fractal_info info) 
{
    switch(info.type) 
    {
        case MANDELBROT_TYPE:
            generate_mandelbrot(info);
        break;
        case JULIA_TYPE:
            generate_julia(info);
        break;
    }
}

int main() 
{
    fractal_info info;
    memset(&info, 0, sizeof(info));
    
    info.w     = 1024;
    info.h     = 768;
    info.depth = 1000;
    info.type  = MANDELBROT_TYPE;
    info.name  = "fractal.ppm";
    
    init_array(&info);
    generate(info);
}