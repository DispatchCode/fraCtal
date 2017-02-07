/* ***********************************************************************
 * Description:  A simple PPM image library with some - in future - improvements.
 *               I'm writing it for one of mine little and future project.
 * -----------------------------------------------------------------------
 * License    :  BSD LICENS (refer to LICENSE)
 * -----------------------------------------------------------------------
 */


#include<stdlib.h>
#include<stdio.h>

#include "include/geom.h"

int w = 1024, h = 768;


/**
 * \brief Test and demo file for PPM image library
 *
 */
 
/*
 *      RUN FIRST TEST
 * ----------------------------------------
 * An image with a "plus" (green and red) will be generated; 
 * the background of the image is black
 *
 */
void first_test()
{
  int i, j;
  
  initialize(w, h);
  for(i=0; i<w; i++) 
  {
    set_pixel_rgb(i,h/2,0,255,0);
  }
  
  for(j=0; j<h; j++) 
  {
    set_pixel_rgb(w/2,j,255,0,0);
  }
  save_image("test_set_pixel.ppm");
}
 
 
/*
 *    SECOND TEST
 * --------------------------------------------
 * The output is the same, but this is generated from a
 * different source: an array of pixels
 *
 */
void second_test()
{
  char *px = (char*) calloc(3+(w*h*3), 8);
  int i = 0, j = h/2;
  
  for(; i<w; i++)
  {
    if(i < w) 
    {
      px[3 * (i + j * w)+1]  = 255;
    }
  }
  
  i = w/2;
  j = 0;
  for(; j<h; j++)
  {
    if(j+1 < h) 
    {
      px[3 * (i + j * w)] = 255;
    }
  }
  
  initialize(w,h);
  set_pixel_array(px);

  save_image("test_set_array.ppm");
  free(px);
}


/*
 *   THIRD TEST
 * ----------------------------------------------------------
 * Same "plus" sign, but on a white background
 *
 */
void third_test()
{
  int i, j;
  
  initialize(w, h);
  set_background_rgb(255, 255, 255);
  
  for(i=0; i<w; i++) 
  {
    set_pixel_rgb(i,h/2,0,255,0);
  }
  
  for(j=0; j<h; j++) 
  {
    set_pixel_rgb(w/2,j,255,0,0);
  }
  save_image("test_set_bg_pixel.ppm");
}


/*
 * FOURTH TEST
 * ---------------------------------------------------------------
 * Like before with 2nd and 4rd square colored in blue and white
 *
 */
 
void fourth_test()
{
  int i, j;
  
  initialize(w, h);
  
  rectangle rect;
  rect.x = 0;
  rect.y = 0;
  rect.b = w/2;
  rect.h = h/2;
  set_background_rect_rgb(rect, 0, 0, 255);
  
  rect.x = w/2;
  rect.y = h/2;
  rect.b = w;
  rect.h = h;
  set_background_rect_rgb(rect, 255, 255, 255);
  
  
  for(i=0; i<w; i++) 
  {
    set_pixel_rgb(i,h/2,0,255,0);
  }
  
  for(j=0; j<h; j++) 
  {
    set_pixel_rgb(w/2,j,255,0,0);
  }
  save_image("test_set_bg_pixel.ppm");
}

int main() 
{
  
  //first_test();
  //second_test();
  //third_test();
  fourth_test();

  return 0;
}