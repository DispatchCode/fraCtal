#include <stdio.h>
#include <time.h>
#include "include/mandelbrot.h"

int main()
{
    size_t w = 1920, h = 1080;

    clock_t initial_time = clock();

    mandelbrot_info fractal_info;
    init(w,h,4, 2000, &fractal_info);
    generation(fractal_info, "mandelbrot.png");

    printf("\n\nCPU time elapsed (s): %lf\n", (double)(clock() - initial_time) / CLOCKS_PER_SEC);

    return 0;
}