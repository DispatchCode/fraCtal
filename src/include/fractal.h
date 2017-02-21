#include "complex.h"

#define  MANDELBROT_TYPE  0x00
#define  JULIA_TYPE       0x01

#define  INDEX(x, y, w) (3 * (x + y * w))


typedef struct _fractal_info 
{
    int   w;
    int   h;
    int   depth;
    int   type;
    char  *name;
    double x0;
    double y0;
    double x1;
    double y1;
    double re_julia;
    double im_julia;
    
} fractal_info;

typedef struct _fractal_image 
{
    char *pixels;
    
} fractal_image;
fractal_image fractal;


extern void init_array(fractal_info *info);
extern int depth_calculation(fractal_info info, double scaled_x, double scaled_y, complex zn, double *mod);
extern void generate_fractal(fractal_info info);