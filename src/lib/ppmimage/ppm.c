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

void save_image(char *imageName, char* pixels, size_t w, size_t h)
{  
  FILE *file = fopen(imageName, "wb");
  fprintf(file, "%s%d%s%d%s", "P6\n#DispatchCode PPMImageLib\n", w, " ", h, "\n255\n");
  fwrite(pixels, sizeof(char), 3+(w*h*3), file);
  
  fclose(file);
}