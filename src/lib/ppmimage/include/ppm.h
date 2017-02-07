/*
 * PPM header lib definitions
 * The Image state is maintained internally
 *
 */

/** @ppm.h */
 
#include "geom.h"


/**
 * \def    index(X,Y,W)
 * \brief  A macro for access a pixel array using (x,y) coords
 */
#define INDEX(x,y,w) (3 * (x + y * w))

/**
 * \def    R(color)
 * \brief  Get the red channel from an int color
 */
#define R(color) (color >> 0x10 & 0xFF)
/**
 * \def    G(color)
 * \brief  Get the green channel from an int color
 */
#define G(color) (color >> 0x08 & 0xFF)

/**
 * \def    B(color)
 * \brief  Get the blue channel from an int color
 */
#define B(color) (color & 0xFF)


typedef struct _ppm_image
{
  char *pixels;
  int  w;
  int  h;
} ppm_image;
ppm_image image;


//
// Function definitions
//


/**
 * \brief The same as before (refer to set_pixel_rgb() )
 *
 * @param color   packed color in rgb format
 */
void set_background_int(int);


/**
 * \brief  Set background color of the image
 *
 */
void set_background_rgb(int, int, int);


/**
 * \brief  <p>Set background color in specified <i>rect</i> area</p>
 *
 */
void set_background_rect_rgb(rectangle, int, int, int);


/**
 *\brief <p><b>NOTE</b> The function assumes that a previously called to the function initialize() is occurred (for performance reason)
 *\brief Set pixel color at the (x,y) location</p>
 *
 * @param  x  x axys position
 * @param  y  y axys position
 * @param  r  red color
 * @param  g  green color
 * @param  b  blue color
 *
 */
void set_pixel_rgb(int, int, int, int, int);


/**
 * \brief <p><b>WARNING!</b> call this only after a previous initialization calling (initialize())</p>
 *
 * @param pixels   pixels array
 */
void set_pixel_array(char*);


void set_pixel_int(int, int, int);


/**
 * \brief  Save Image in ppm format (eg. with ppm extension)
 *
 * @param  imageName   the name of the image
 */
void save_image(char*);


/**
 * \brief <p>Memory allocation and struct initialization</p>
 *
 * @param  w  width  of the image
 * @param  h  height of the image
 */
void initialize(int, int);


/**
 * \brief  Destroy pixel array without save image
 *
 */
void destroy();