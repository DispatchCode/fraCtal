typedef struct _complex_number 
{
    double re;
    double im;    
} complex;

extern complex complex_sum(complex a, complex b);
extern complex complex_mul(complex a, complex b);
extern double complex_abs(complex a);