#include "../System.h"
#include <stdlib.h>
#include <memory.h>

#include "interframe.hpp"

/*
 * Thanks to Kawaks' Mr. K for the code

   Incorporated into vba by Anthony Di Franco

   2020-2-8 - Modified for libretro
*/

static uint8_t *frm1    = NULL;
static uint8_t *frm2    = NULL;
static uint8_t *frm3    = NULL;
static bool initialized = false;

static void Init(void)
{
   frm1 = (uint8_t *)calloc(256 * 240, 4);
   // 1 frame ago
   frm2 = (uint8_t *)calloc(256 * 240, 4);
   // 2 frames ago
   frm3 = (uint8_t *)calloc(256 * 240, 4);
   // 3 frames ago
   initialized = true;
}

void InterframeCleanup(void)
{
   //Hack to prevent double freeing *It looks like this is not being called in a thread safe manner)
   if (frm1)
      free(frm1);
   if (frm2 && (frm1 != frm2))
      free(frm2);
   if (frm3 && (frm1 != frm3) && (frm2 != frm3))
      free(frm3);
   frm1 = frm2 = frm3 = NULL;
   initialized = false;
}

void SmartIB(pixFormat *srcPtr, int width, int height)
{
   if (initialized == 0)
      Init();

   pixFormat colorMask = ~RGB_LOW_BITS_MASK;
   pixFormat *src0     = srcPtr;      
   uint32_t pos        = 0;

   for (int j = 0; j < height; j++)
   {
      for (int i = 0; i < width; i++)
      {
         pixFormat color = *(src0 + pos);
         pixFormat src1  = *((pixFormat *)frm1 + pos);
         pixFormat src2  = *((pixFormat *)frm2 + pos);
         pixFormat src3  = *((pixFormat *)frm3 + pos);
         *(src0 + pos) = (src1 != src2) && (src3 != color) &&
            ((color == src2) || (src1 == src3)) ?
            (((color & colorMask) >> 1) + ((src1 & colorMask) >> 1)) : color;
         // oldest buffer now holds newest frame
         *((pixFormat *)frm3 + pos) = color;
         pos++;
      }
   }

   // Swap buffers around
   uint8_t *temp = frm1;
   frm1 = frm3;
   frm3 = frm2;
   frm2 = temp;
}

void MotionBlurIB(pixFormat *srcPtr, int width, int height)
{
   if (initialized == 0)
      Init();
   
   pixFormat colorMask = ~RGB_LOW_BITS_MASK;
   pixFormat *src0     = srcPtr;
   uint32_t pos        = 0;

   for (int j = 0; j < height; j++)
   {
      for (int i = 0; i < width; i++)
      {
         pixFormat color = *(src0 + pos);
         pixFormat src1 = *((pixFormat *)frm1 + pos);
         *(src0 + pos) = (((color & colorMask) >> 1) + ((src1 & colorMask) >> 1));
         *((pixFormat *)frm1 + pos) = color;
         pos++;
      }
   }
}
