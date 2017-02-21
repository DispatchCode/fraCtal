#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/fractal.h"


/*
 * 
 *
 *
 */


/*
 * Point of interest: 0.37 0.40 0.21 0.26
 */
void init_array(fractal_info *info) 
{
    if(!info->x0) info->x0 = 2.0;
    if(!info->y0) info->y0 = 2.0;
    if(!info->x1) info->x1 = -2.0;
    if(!info->y1) info->y1 = -2.0;
    
    if(info->type == JULIA_TYPE)
    {
        if(!info->re_julia) info->re_julia = -0.8;
        if(!info->im_julia) info->im_julia = 0.156;
    }
    
    
    fractal.pixels = (char*) calloc(3 + (info->w * info->h * 3), 8);
}

int depth_calculation(fractal_info info, double scaled_x, double scaled_y, complex c, double *abs)
{
    complex z;
    z.re = scaled_x;
    z.im = scaled_y;

    int i;
    for(i = 0; i < info.depth; i++)
    {
        z    = complex_sum(complex_mul(z, z), c);
        *abs = complex_abs(z);

        if(*abs > 4.0) {
            break;
        }
    }

    return i;
}

void generate_fractal(fractal_info info) 
{
    int i, j;
    
    for(j = 0; j < info.h; j++)
    {
        double y = info.y1 + (((double)j * (info.y0-info.y1)) / (double)info.h);

        for(i = 0; i < info.w; i++)
        {
            double x = info.x1 + (((double)i * (info.x0-info.x1)) / (double)info.w);
            
            complex c;
            switch(info.type)
            {
                case MANDELBROT_TYPE:
                    c.re = x;
                    c.im = y;
                break;
                case JULIA_TYPE:
                    c.re = info.re_julia;
                    c.im = info.im_julia;
                break;
            }

            double abs;
            int iteration = depth_calculation(info, x, y, c, &abs);

            if(abs <= 4.0) 
            {
                fractal.pixels[INDEX(i, j, info.w)    ] = 0;
                fractal.pixels[INDEX(i, j, info.w) + 1] = 0;
                fractal.pixels[INDEX(i, j, info.w) + 2] = 0;
            }
            else
            {

                // RED
                int r =  iteration<<2;
                int g =  iteration>>1;
                int b =  iteration>>2;
                
                r = (r>255) ? 255 : r;
                g = (g>128) ? 128 : g;
                b = (b>128) ? 128 : b;
        
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