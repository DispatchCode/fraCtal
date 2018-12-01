#include<stdio.h>
#include<stdlib.h>
#include<time.h>
#include<string.h>
#include<assert.h>

#include "include/fractal.h"

/*
 *  compilation: gcc main_fractal.c fractal.c lib/ppmimage/ppm.c complex.c  -o fractal -O2
 *  compilation: gcc main_fractal.c fractal_multithread.c lib/ppmimage/ppm.c complex.c  -o fractal -O2
 *
 */

int main() 
{
    fractal_info info;
    memset(&info, 0, sizeof(info));
       
    printf("\tfraCtal generator v.1.0\n\n");
    printf("For generating the fractal there are a few information required:\n");
    printf("\t- x0, y0, x1, y1: this values are constants and rapresent the cartesian plane;\n");
    printf("\t- w, h          : width, height of the image;\n");
    printf("\t- depth         : number of cycles, a greater value is more precise;\n");
    printf("\t- re, im        : required only with Julia (the C number is constant).\n\n");
    printf("As I said before, the image is generated in PPM format. You can display it using\nthird party software or you can convert it in PNG.\n\n");

    printf("==== THE FRACTAL SET ====\n");
    printf("0. Mandelbrot;\n");
    printf("1. Julia.\n\n");
    scanf("%d", &info.type);
    
    assert(info.type == 0 || info.type == 1);
    
    printf("\n==== THE PLANE ====\n");
    printf("x0 (0 for default, 2.0): ");
    scanf("%lf", &info.x0);
    printf("y0 (0 for default, 2.0): ");
    scanf("%lf", &info.y0);
    printf("x1 (0 for default, -2.0): ");
    scanf("%lf", &info.x1);
    printf("y1 (0 for default, -2.0): ");
    scanf("%lf", &info.y1);

    printf("\n==== THE SIZE ====\n");
    printf("W: ");
    scanf("%d", &info.w);
    printf("H: ");
    scanf("%d", &info.h);
    
    printf("\n==== ITERATIONS ====\n");
    printf("A greater value is more precise but the execution time will be increasing faster\n");
    printf("Iterations: ");
    scanf("%d", &info.depth);
    
    if(info.type == JULIA_TYPE)
    {
        printf("\n==== ENTER THE C VALUE ====\n");
        printf("The C number is the point where the function could or could not converge\n");
        printf("Real part (0 for default, -0.8): ");
        scanf("%lf", &info.re_julia);
        printf("Imaginary part (0 for default, 0.156): ");
        scanf("%lf", &info.im_julia);
    }
    
    info.name  = "fractal.ppm";
    init_array(&info);
    
    printf("\nProcess started...\n");
    printf("The colors may be adjusted only from the source code.\n");
    
    clock_t initial_time = clock();
    generate_fractal(info);
    
    printf("\n\nCPU time elapsed (s): %lf\n", (double)(clock() - initial_time) / CLOCKS_PER_SEC);
}