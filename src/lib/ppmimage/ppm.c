/*
Copyright (c) 2016, Marco 'DispatchCode' C.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
   
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
   
3. All advertising materials mentioning features or use of this software
   must display the following acknowledgement:
   This product includes software developed by Marco 'DispatchCode' C.

THIS SOFTWARE IS PROVIDED ''AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef  _PPM_IMAGE

#define  _PPM_IMAGE

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "include/ppm.h"

#endif


void initialize(int w, int h) 
{
  if(image.pixels == NULL)
  {    
    image.pixels = (char*) calloc(3+(w*h*3), 8);
    image.w      = w;
    image.h      = h;
  }
}

void set_pixel_rgb(int x, int y, int r, int g, int b) 
{
  if(x >=0 && y>=0 && x<=image.w && y<=image.h) 
  {
    image.pixels[INDEX(x,y,image.w)  ] = (char) r;
    image.pixels[INDEX(x,y,image.w)+1] = (char) g;
    image.pixels[INDEX(x,y,image.w)+2] = (char) b;
  }
}

void set_pixel_int(int x, int y, int color)
{
  set_pixel_rgb(x, y, R(color), G(color), B(color));
}

void set_pixel_array(char* pixels)
{
  assert(image.pixels != NULL);
  memcpy(&image.pixels, &pixels, sizeof(image.pixels));
}

void save_image(char *imageName) 
{  
  FILE *file = fopen(imageName, "wb");
  fprintf(file, "%s%d%s%d%s", "P6\n#DispatchCode PPMImageLib\n", image.w, " ", image.h, "\n255\n");  
  fwrite(image.pixels, sizeof(char), 3+(image.w*image.h*3), file);
  
  fclose(file);
  free(image.pixels);
}

void set_background_rect_rgb(rectangle rect, int r, int g, int b)
{
  assert(rect.b <= image.w && rect.h <= image.h);
  
  int x = rect.x;
  while(x++ < rect.b)
  {
    int y = rect.y;
    while(y++ < rect.h)
    {
      set_pixel_rgb(x,y,r,g,b);
    }  
  }
}

void set_background_rgb(int r, int g, int b)
{
  // The "rect" area is the entire image
  rectangle rect;
  rect.y = 0;
  rect.x = 0;
  rect.b = image.w;
  rect.h = image.h;
  
  set_background_rect_rgb(rect, r, g, b);
}

void set_background_int(int color)
{
  set_background_rgb(R(color), G(color), B(color));
}

void destroy()
{
  assert(image.pixels != NULL);
  free(image.pixels);
}