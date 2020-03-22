# fraCtal

*fraCtal* generates fractals of any dimension that you want. It can generate fractals belonging at two sets: Mandelbrot and Julia.

*New update (29/12/2018)!*
---
- [x] multithread support
- [x] initial support to custom color palette
- [x] removed unused code (from PPMlib and other)
- [x] Julia was removed
- [ ] CUI menu 

Comparison between the old and the new execution time:

 1-thread  |  4-thread  |   (x0, x1, y0, y1)      |
-----------|------------|--------------------------|
  166.30s   |  57.37s    | (0.37, 0.40, 0.21, 0.16) |
198.322s    |  111.56s    | (0.37, 0.40, 0.21, 0.26) |
 46.11s     |  21.60s     | (2.0, -2.0, 1.7, -1.7)   |
 
---

You can specify another number of threads in *main.c*.

Last but not least, now is possible load colors (R,G,B) from a text file with the following format:

```
size (number of lines)
r,g,b
r,g,b
.....
```

as an example, consider the image on Wikipedia: this are the colors:

```
16
66,30,15
25,7,26
9,1,47
4,4,73
0,7,100
12,44,138
24,82,177
57, 125,209
134,181,229
211,236,248
241,233,191
248,201,95
255,170,0
204,128,0
153,87,0
106,52,3
```

you can find it the palette.txt file.
As a result we get:

![mandelbrot_wiki](https://user-images.githubusercontent.com/4256708/77239653-700bfe00-6bdd-11ea-8027-9d8dc4a2b9fc.png)


Uncomment PALETTE in mandelbrot.h if you want use them.

## Mathematical formula and explanation

On the web there are a lot of explanations about this formula so I don't want to be boring or maybe not totally precise.

![](http://warp.povusers.org/Mandelbrot/eqn1.png)

*Z* and *C* are two complex numbers. Complex numbers are composed by two parts: a real part, and a imaginary part. The formula tell to us that if the value of the function Zn for the value c is not infinite when n approaches infinity, then c belongs to the set. If C belongs to the set, then we choose the black color; otherwise the color will be white.

## Besides mathematics: the code

To determine which will be the color (eg. black or not) we can't obviously iterate to infinity. Luckily maths tell that for definition if the absolute value of Z gets bigger of 2 it will never return closer to 2. So thanks to that we can iterate N times; N could be little but it depends from the number of pixels of the image.

This is my code:
```
double y = info.y1 + (((double)j * (info.y0-info.y1)) / (double)info.h);
double x = info.x1 + (((double)i * (info.x0-info.x1)) / (double)info.w);
```

Here *x* and *y* are the coords of the 2D plan (scaled). In this manner y and x are the imaginary part and the real part of the complex number *C* seen before (the point that we need to know if belong or not to the set).

The N number is chosen by user. We must just iterate till this value is reached or break the loop if the square root of the abs value is greater then 2; we can simplify this operation removing the sqrt and check for 4.

Here the code:
```
for(i = 0; i < info.depth; i++)
{
    z    = complex_sum(complex_mul(z, z), c);
    *abs = complex_abs(z);

    if(*abs > 4.0) {
        break;
    }
}
```

Just to summarize: if abs is greater than 4, the *C* point is not part of the set and it will be left white (but is common give another color based on the amount of iterations); if the value is below 4 then the common color is black because is part of the set.

## The Area

The area is determined by x0, x1, y0 and y1. They represent the cartesian plane and they are the real part and imaginary part.
This four numbers are important because - how we will see - we can use them to zoomin or zoomout in the fractal.

## The Colors

In the first part of this README I had talked about the Zn function and his result that tell to us if the value *C* belongs to the set or not. When *C* not belongs to the set we can set a color based on the numbers of the iterations. The intensity of the color differ based on the number of iterations.

If you want to change the primary color (eg, make the fractal red, green or blue or whatever) you must edit the code.

Here some examples; all the images are generated using the same "zoom" (default zooming): x0=2.0, x1=-2.0, y0=2.0, y1=-2.0 and the same iterations, 2.000:

For example, to get this blue, you must use:

```
int r =  iteration>>2;
int g =  iteration>>1;
int b =  iteration<<3;

r = (r>128) ? 128 : r;
g = (g>128) ? 128 : g;
b = (b>255) ? 255 : b;
```

![mandelbrot_blue](https://user-images.githubusercontent.com/4256708/77239813-0db3fd00-6bdf-11ea-97c1-982e35653d56.png)


For the green version, you must change the code in this way:

```
int r =  iteration>>1;
int g =  iteration<<3;
int b =  iteration>>1;

r = (r>128) ? 128 : r;
g = (g>255) ? 255 : g;
b = (b>128) ? 128 : b;
```

![mandelbrot_green](https://user-images.githubusercontent.com/4256708/77239755-7babf480-6bde-11ea-9ab9-04f336782bac.png)

For the red/orange version:

```
int r =  iteration<<2;
int g =  iteration>>1;
int b =  iteration>>2;

r = (r>255) ? 255 : r;
g = (g>128) ? 128 : g;
b = (b>128) ? 128 : b;
```

![mandelbrot_red1](https://user-images.githubusercontent.com/4256708/77239746-6767f780-6bde-11ea-8435-dae2557dad95.png)

Obviously this is not the only way to set a color.

We can also zoom in, how I said before. Let's take a look here, x0=0.37, x1=0.40, y0=0.21, y1=0.26 using the same red color:

![mandel_red](https://user-images.githubusercontent.com/4256708/77239780-9c744a00-6bde-11ea-996b-3bfe71447003.png)

How you can see the color make the difference!

# Gallery

![fractal](https://user-images.githubusercontent.com/4256708/77239841-4fdd3e80-6bdf-11ea-8562-a1b9c01c8f3d.png)

![mandelbroat_blue_zoom](https://user-images.githubusercontent.com/4256708/77240063-f7f40700-6be1-11ea-9f38-c13ff1490270.png)

![mandelbrot_acquamarina](https://user-images.githubusercontent.com/4256708/77239849-64b9d200-6bdf-11ea-98d5-e276d224ca15.png)

![mandelbrot_blue_green](https://user-images.githubusercontent.com/4256708/77239862-8024dd00-6bdf-11ea-8ad0-5e8889790ffb.png)

![mandelbrot_light_blue](https://user-images.githubusercontent.com/4256708/77239869-96cb3400-6bdf-11ea-9210-5a842de806b7.png)

![mandelbrot_orange](https://user-images.githubusercontent.com/4256708/77239872-a77baa00-6bdf-11ea-9c47-882b53eb7d77.png)