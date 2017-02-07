/*
 * Just a simplified implementation of complex numbers for fractal generation
 *
 */
 
#include  "include/complex.h"

complex complex_sum(complex a, complex b)
{
    complex c;
    c.re = a.re + b.re;
    c.im = a.im + b.im;
    return c;
}

complex complex_mul(complex a, complex b)
{
    complex c;
    c.re = a.re * b.re - a.im * b.im;
    c.im = a.re * b.im + a.im * b.re;
    return c;
}

double complex_abs(complex a)
{
    return a.re * a.re + a.im * a.im;
}