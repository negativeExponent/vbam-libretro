#include "GBAGfx.h"
#include "../System.h"
#include <string.h>

int coeff[32] = {
   0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
   16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16
};

uint32_t line0[240];
uint32_t line1[240];
uint32_t line2[240];
uint32_t line3[240];
uint32_t lineOBJ[240];
uint32_t lineOBJWin[240];
bool gfxInWin0[240];
bool gfxInWin1[240];
int lineOBJpixleft[128];

lcd_background_t lcd_bg[4];

static inline void gfxClearArray(uint32_t* array)
{
   for (int i = 0; i < 240;)
   {
      *array++ = 0x80000000;
      *array++ = 0x80000000;
      *array++ = 0x80000000;
      *array++ = 0x80000000;

      *array++ = 0x80000000;
      *array++ = 0x80000000;
      *array++ = 0x80000000;
      *array++ = 0x80000000;

      *array++ = 0x80000000;
      *array++ = 0x80000000;
      *array++ = 0x80000000;
      *array++ = 0x80000000;

      *array++ = 0x80000000;
      *array++ = 0x80000000;
      *array++ = 0x80000000;
      *array++ = 0x80000000;

      i += 16;
   }
}

void forceBlankLine(pixFormat* lineMix)
{
   for (int x = 0; x < 240; ++x)
   {
      lineMix[x] = MAKECOLOR(0x7fff);
   }
}

static int textModeSizeMapX[4] = { 256, 512, 256, 512 };
static int textModeSizeMapY[4] = { 256, 256, 512, 512 };

#ifdef TILED_RENDERING
#ifdef _MSC_VER
union uint8_th {
   __pragma(pack(push, 1));
   struct
   {
#ifdef MSB_FIRST
      /* 4*/ unsigned char hi : 4;
      /* 0*/ unsigned char lo : 4;
#else
      /* 0*/ unsigned char lo : 4;
      /* 4*/ unsigned char hi : 4;
#endif
   } __pragma(pack(pop));
   uint8_t val;
};
#else // !_MSC_VER
union uint8_th {
   struct
   {
#ifdef MSB_FIRST
      /* 4*/ unsigned char hi : 4;
      /* 0*/ unsigned char lo : 4;
#else
      /* 0*/ unsigned char lo : 4;
      /* 4*/ unsigned char hi : 4;
#endif
   } __attribute__((packed));
   uint8_t val;
};
#endif

union TileEntry {
   struct
   {
#ifdef MSB_FIRST
      /*14*/ unsigned palette : 4;
      /*13*/ unsigned vFlip : 1;
      /*12*/ unsigned hFlip : 1;
      /* 0*/ unsigned tileNum : 10;
#else
      /* 0*/ unsigned tileNum : 10;
      /*12*/ unsigned hFlip : 1;
      /*13*/ unsigned vFlip : 1;
      /*14*/ unsigned palette : 4;
#endif
   };
   uint16_t val;
};

struct TileLine
{
   uint32_t pixels[8];
};

typedef const TileLine (*TileReader)(const uint16_t*, const int, const uint8_t*, uint16_t*, const uint32_t);

static inline void gfxDrawPixel(uint32_t* dest, const uint8_t color, const uint16_t* palette, const uint32_t prio)
{
   *dest = color ? (READ16LE(&palette[color]) | prio) : 0x80000000;
}

inline const TileLine gfxReadTile(const uint16_t* screenSource, const int yyy, const uint8_t* charBase, uint16_t* palette, const uint32_t prio)
{
   TileEntry tile;
   tile.val = READ16LE(screenSource);

   int tileY = yyy & 7;
   if (tile.vFlip)
      tileY = 7 - tileY;
   TileLine tileLine;

   const uint8_t* tileBase = &charBase[tile.tileNum * 64 + tileY * 8];

   if (!tile.hFlip)
   {
      gfxDrawPixel(&tileLine.pixels[0], tileBase[0], palette, prio);
      gfxDrawPixel(&tileLine.pixels[1], tileBase[1], palette, prio);
      gfxDrawPixel(&tileLine.pixels[2], tileBase[2], palette, prio);
      gfxDrawPixel(&tileLine.pixels[3], tileBase[3], palette, prio);
      gfxDrawPixel(&tileLine.pixels[4], tileBase[4], palette, prio);
      gfxDrawPixel(&tileLine.pixels[5], tileBase[5], palette, prio);
      gfxDrawPixel(&tileLine.pixels[6], tileBase[6], palette, prio);
      gfxDrawPixel(&tileLine.pixels[7], tileBase[7], palette, prio);
   }
   else
   {
      gfxDrawPixel(&tileLine.pixels[0], tileBase[7], palette, prio);
      gfxDrawPixel(&tileLine.pixels[1], tileBase[6], palette, prio);
      gfxDrawPixel(&tileLine.pixels[2], tileBase[5], palette, prio);
      gfxDrawPixel(&tileLine.pixels[3], tileBase[4], palette, prio);
      gfxDrawPixel(&tileLine.pixels[4], tileBase[3], palette, prio);
      gfxDrawPixel(&tileLine.pixels[5], tileBase[2], palette, prio);
      gfxDrawPixel(&tileLine.pixels[6], tileBase[1], palette, prio);
      gfxDrawPixel(&tileLine.pixels[7], tileBase[0], palette, prio);
   }

   return tileLine;
}

inline const TileLine gfxReadTilePal(const uint16_t* screenSource, const int yyy, const uint8_t* charBase, uint16_t* palette, const uint32_t prio)
{
   TileEntry tile;
   tile.val = READ16LE(screenSource);

   int tileY = yyy & 7;
   if (tile.vFlip)
      tileY = 7 - tileY;
   palette += tile.palette * 16;
   TileLine tileLine;

   const uint8_th* tileBase = (uint8_th*)&charBase[tile.tileNum * 32 + tileY * 4];

   if (!tile.hFlip)
   {
      gfxDrawPixel(&tileLine.pixels[0], tileBase[0].lo, palette, prio);
      gfxDrawPixel(&tileLine.pixels[1], tileBase[0].hi, palette, prio);
      gfxDrawPixel(&tileLine.pixels[2], tileBase[1].lo, palette, prio);
      gfxDrawPixel(&tileLine.pixels[3], tileBase[1].hi, palette, prio);
      gfxDrawPixel(&tileLine.pixels[4], tileBase[2].lo, palette, prio);
      gfxDrawPixel(&tileLine.pixels[5], tileBase[2].hi, palette, prio);
      gfxDrawPixel(&tileLine.pixels[6], tileBase[3].lo, palette, prio);
      gfxDrawPixel(&tileLine.pixels[7], tileBase[3].hi, palette, prio);
   }
   else
   {
      gfxDrawPixel(&tileLine.pixels[0], tileBase[3].hi, palette, prio);
      gfxDrawPixel(&tileLine.pixels[1], tileBase[3].lo, palette, prio);
      gfxDrawPixel(&tileLine.pixels[2], tileBase[2].hi, palette, prio);
      gfxDrawPixel(&tileLine.pixels[3], tileBase[2].lo, palette, prio);
      gfxDrawPixel(&tileLine.pixels[4], tileBase[1].hi, palette, prio);
      gfxDrawPixel(&tileLine.pixels[5], tileBase[1].lo, palette, prio);
      gfxDrawPixel(&tileLine.pixels[6], tileBase[0].hi, palette, prio);
      gfxDrawPixel(&tileLine.pixels[7], tileBase[0].lo, palette, prio);
   }

   return tileLine;
}

static inline void gfxDrawTile(const TileLine& tileLine, uint32_t* line)
{
   memcpy(line, tileLine.pixels, sizeof(tileLine.pixels));
}

static inline void gfxDrawTileClipped(const TileLine& tileLine, uint32_t* line, const int start, int w)
{
   memcpy(line, tileLine.pixels + start, w * sizeof(uint32_t));
}

template <TileReader readTile>
static void gfxDrawTextScreen(lcd_background_t* bg, uint16_t* palette, uint32_t* line)
{
   uint8_t* charBase = &vram[((bg->control >> 2) & 0x03) * 0x4000];
   uint16_t* screenBase = (uint16_t*)&vram[((bg->control >> 8) & 0x1f) * 0x800];
   uint32_t prio = ((bg->control & 3) << 25) + 0x1000000;

   int screenSize = (bg->control >> 14) & 3;
   int sizeX = textModeSizeMapX[screenSize];
   int sizeY = textModeSizeMapY[screenSize];

   int maskX = sizeX - 1;
   int maskY = sizeY - 1;

   bool mosaicOn = (bg->control & 0x40) ? true : false;

   int xxx = bg->h_offset & maskX;
   int yyy = (bg->v_offset + VCOUNT) & maskY;
   int mosaicX = (MOSAIC & 0x000F) + 1;
   int mosaicY = ((MOSAIC & 0x00F0) >> 4) + 1;

   if (mosaicOn)
   {
      if ((VCOUNT % mosaicY) != 0)
      {
         mosaicY = VCOUNT - (VCOUNT % mosaicY);
         yyy = (bg->v_offset + mosaicY) & maskY;
      }
   }

   if (yyy > 255 && sizeY > 256)
   {
      yyy &= 255;
      screenBase += 0x400;
      if (sizeX > 256)
         screenBase += 0x400;
   }

   int yshift = ((yyy >> 3) << 5);

   uint16_t* screenSource = screenBase + 0x400 * (xxx >> 8) + ((xxx & 255) >> 3) + yshift;
   int x = 0;
   const int firstTileX = xxx & 7;

   // First tile, if clipped
   if (firstTileX)
   {
      gfxDrawTileClipped(readTile(screenSource, yyy, charBase, palette, prio), &line[x], firstTileX, 8 - firstTileX);
      ++screenSource;
      x += 8 - firstTileX;
      xxx += 8 - firstTileX;

      if (xxx == 256 && sizeX > 256)
      {
         screenSource = screenBase + 0x400 + yshift;
      }
      else if (xxx >= sizeX)
      {
         xxx = 0;
         screenSource = screenBase + yshift;
      }
   }

   // Middle tiles, full
   while (x < 240 - firstTileX)
   {
      gfxDrawTile(readTile(screenSource, yyy, charBase, palette, prio), &line[x]);
      ++screenSource;
      xxx += 8;
      x += 8;

      if (xxx == 256 && sizeX > 256)
      {
         screenSource = screenBase + 0x400 + yshift;
      }
      else if (xxx >= sizeX)
      {
         xxx = 0;
         screenSource = screenBase + yshift;
      }
   }

   // Last tile, if clipped
   if (firstTileX)
   {
      gfxDrawTileClipped(readTile(screenSource, yyy, charBase, palette, prio), &line[x], 0, firstTileX);
   }

   if (mosaicOn)
   {
      if (mosaicX > 1)
      {
         int m = 1;
         for (int i = 0; i < 239; ++i)
         {
            line[i + 1] = line[i];
            if (++m == mosaicX)
            {
               m = 1;
               ++i;
            }
         }
      }
   }
}

void gfxDrawTextScreen(lcd_background_t* bg, uint16_t* palette, uint32_t* line)
{
   if (bg->control & 0x80) // 1 pal / 256 col
      gfxDrawTextScreen<gfxReadTile>(bg, palette, line);
   else // 16 pal / 16 col
      gfxDrawTextScreen<gfxReadTilePal>(bg, palette, line);
}
#endif // TILED_RENDERING

#ifndef TILED_RENDERING
void gfxDrawTextScreen(uint16_t control, uint16_t h_offset, uint16_t v_offset, uint32_t* line)
{
   uint16_t* palette = (uint16_t*)paletteRAM;
   uint8_t* charBase = &vram[((control >> 2) & 0x03) * 0x4000];
   uint16_t* screenBase = (uint16_t*)&vram[((control >> 8) & 0x1f) * 0x800];
   uint32_t prio = ((control & 3) << 25) + 0x1000000;

   int screenSize = (control >> 14) & 3;
   int sizeX = textModeSizeMapX[screenSize];
   int sizeY = textModeSizeMapY[screenSize];

   int maskX = sizeX - 1;
   int maskY = sizeY - 1;

   bool mosaicOn = (control & 0x40) ? true : false;

   int xxx = h_offset & maskX;
   int yyy = (v_offset + VCOUNT) & maskY;
   int mosaicX = (MOSAIC & 0x000F) + 1;
   int mosaicY = ((MOSAIC & 0x00F0) >> 4) + 1;

   if (mosaicOn)
   {
      if ((VCOUNT % mosaicY) != 0)
      {
         mosaicY = VCOUNT - (VCOUNT % mosaicY);
         yyy = (v_offset + mosaicY) & maskY;
      }
   }

   if (yyy > 255 && sizeY > 256)
   {
      yyy &= 255;
      screenBase += 0x400;
      if (sizeX > 256)
         screenBase += 0x400;
   }

   int yshift = ((yyy >> 3) << 5);
   if ((control)&0x80)
   {
      uint16_t* screenSource = screenBase + 0x400 * (xxx >> 8) + ((xxx & 255) >> 3) + yshift;
      for (int x = 0; x < 240; x++)
      {
         uint16_t data = READ16LE(screenSource);

         int tile = data & 0x3FF;
         int tileX = (xxx & 7);
         int tileY = yyy & 7;

         if (tileX == 7)
            screenSource++;

         if (data & 0x0400)
            tileX = 7 - tileX;
         if (data & 0x0800)
            tileY = 7 - tileY;

         uint8_t color = charBase[tile * 64 + tileY * 8 + tileX];

         line[x] = color ? (READ16LE(&palette[color]) | prio) : 0x80000000;

         xxx++;
         if (xxx == 256)
         {
            if (sizeX > 256)
               screenSource = screenBase + 0x400 + yshift;
            else
            {
               screenSource = screenBase + yshift;
               xxx = 0;
            }
         }
         else if (xxx >= sizeX)
         {
            xxx = 0;
            screenSource = screenBase + yshift;
         }
      }
   }
   else
   {
      uint16_t* screenSource = screenBase + 0x400 * (xxx >> 8) + ((xxx & 255) >> 3) + yshift;
      for (int x = 0; x < 240; x++)
      {
         uint16_t data = READ16LE(screenSource);

         int tile = data & 0x3FF;
         int tileX = (xxx & 7);
         int tileY = yyy & 7;

         if (tileX == 7)
            screenSource++;

         if (data & 0x0400)
            tileX = 7 - tileX;
         if (data & 0x0800)
            tileY = 7 - tileY;

         uint8_t color = charBase[(tile << 5) + (tileY << 2) + (tileX >> 1)];

         if (tileX & 1)
         {
            color = (color >> 4);
         }
         else
         {
            color &= 0x0F;
         }

         int pal = (data >> 8) & 0xF0;
         line[x] = color ? (READ16LE(&palette[pal + color]) | prio) : 0x80000000;

         xxx++;
         if (xxx == 256)
         {
            if (sizeX > 256)
               screenSource = screenBase + 0x400 + yshift;
            else
            {
               screenSource = screenBase + yshift;
               xxx = 0;
            }
         }
         else if (xxx >= sizeX)
         {
            xxx = 0;
            screenSource = screenBase + yshift;
         }
      }
   }
   if (mosaicOn)
   {
      if (mosaicX > 1)
      {
         int m = 1;
         for (int i = 0; i < 239; i++)
         {
            line[i + 1] = line[i];
            m++;
            if (m == mosaicX)
            {
               m = 1;
               i++;
            }
         }
      }
   }
}
#endif // !__TILED_RENDERING

template <typename T>
static inline T MAX(T a, T b)
{
   return (a > b) ? a : b;
}

template <typename T>
static inline T MIN(T a, T b)
{
   return (a < b) ? a : b;
}

static int rotationModeSizeMap[4] = { 128, 256, 512, 1024 };

void gfxDrawRotScreen(lcd_background_t* bg, uint16_t* palette, uint32_t* line)
{
   uint8_t* charBase = &vram[((bg->control >> 2) & 0x03) * 0x4000];
   uint8_t* screenBase = (uint8_t*)&vram[((bg->control >> 8) & 0x1f) * 0x800];
   int prio = ((bg->control & 3) << 25) + 0x1000000;

   int screenSize = (bg->control >> 14) & 3;
   int sizeX = rotationModeSizeMap[screenSize];
   int sizeY = rotationModeSizeMap[screenSize];

   int maskX = sizeX - 1;
   int maskY = sizeY - 1;

   int yshift = screenSize + 4;

   int realX = bg->x_pos;
   int realY = bg->y_pos;

   if (bg->control & 0x40)
   {
      int mosaicY = ((MOSAIC & 0xF0) >> 4) + 1;
      int y = (VCOUNT % mosaicY);
      realX -= y * bg->dmx;
      realY -= y * bg->dmy;
   }

   gfxClearArray(line);
   if (bg->control & 0x2000) // Wrap around
   {
      if (bg->dx > 0 && bg->dy == 0) // Common subcase: no rotation or flipping
      {
         int yyy = (realY >> 8) & maskY;
         int yyyshift = (yyy >> 3) << yshift;
         int tileY = yyy & 7;
         int tileYshift = (tileY << 3);

         for (int x = 0; x < 240; ++x)
         {
            int xxx = (realX >> 8) & maskX;

            int tile = screenBase[(xxx >> 3) | yyyshift];

            int tileX = (xxx & 7);

            uint8_t color = charBase[(tile << 6) | tileYshift | tileX];

            if (color)
               line[x] = (READ16LE(&palette[color]) | prio);

            realX += bg->dx;
         }
      }
      else
      {
         for (int x = 0; x < 240; ++x)
         {
            int xxx = (realX >> 8) & maskX;
            int yyy = (realY >> 8) & maskY;

            int tile = screenBase[(xxx >> 3) + ((yyy >> 3) << yshift)];

            int tileX = (xxx & 7);
            int tileY = yyy & 7;

            uint8_t color = charBase[(tile << 6) + (tileY << 3) + tileX];

            if (color)
               line[x] = (READ16LE(&palette[color]) | prio);

            realX += bg->dx;
            realY += bg->dy;
         }
      }
   }
   else // Culling
   {
      if (bg->dx > 0 && bg->dy == 0) // Common subcase: no rotation or flipping
      {
         int yyy = (realY >> 8);

         if (yyy < 0 || yyy >= sizeY)
            goto skipLine;

         int x0 = MAX<int32_t>(0, (int32_t)(+(-realX + bg->dx - 1)) / bg->dx);
         int x1 = MIN<int32_t>(240, (int32_t)((sizeX << 8) + (-realX + bg->dx - 1)) / bg->dx);

         if (x1 < 0)
            goto skipLine;

         int yyyshift = (yyy >> 3) << yshift;
         int tileY = yyy & 7;
         int tileYshift = (tileY << 3);

         realX += bg->dx * x0;

         for (int x = x0; x < x1; ++x)
         {
            int xxx = (realX >> 8);

            int tile = screenBase[(xxx >> 3) | yyyshift];
            int tileX = (xxx & 7);

            uint8_t color = charBase[(tile << 6) | tileYshift | tileX];

            if (color)
               line[x] = (READ16LE(&palette[color]) | prio);

            realX += bg->dx;
         }
      }
      else
      {
         for (int x = 0; x < 240; ++x)
         {
            int xxx = (realX >> 8);
            int yyy = (realY >> 8);

            if (xxx < 0 || yyy < 0 || xxx >= sizeX || yyy >= sizeY)
            {
               //line[x] = 0x80000000;
            }
            else
            {
               int tile = screenBase[(xxx >> 3) + ((yyy >> 3) << yshift)];

               int tileX = (xxx & 7);
               int tileY = yyy & 7;

               uint8_t color = charBase[(tile << 6) + (tileY << 3) + tileX];

               if (color)
                  line[x] = (READ16LE(&palette[color]) | prio);
            }
            realX += bg->dx;
            realY += bg->dy;
         }
      }
   }

skipLine:

   if (bg->control & 0x40)
   {
      int mosaicX = (MOSAIC & 0xF) + 1;
      if (mosaicX > 1)
      {
         int m = 1;
         for (int i = 0; i < 239; ++i)
         {
            line[i + 1] = line[i];
            if (++m == mosaicX)
            {
               m = 1;
               ++i;
            }
         }
      }
   }
}

void gfxDrawRotScreen16Bit(lcd_background_t* bg)
{
   uint16_t* screenBase = (uint16_t*)&vram[0];
   int prio = ((bg->control & 3) << 25) + 0x1000000;
   int sizeX = 240;
   int sizeY = 160;

   int realX = bg->x_pos;
   int realY = bg->y_pos;

   if (bg->control & 0x40)
   {
      int mosaicY = ((MOSAIC & 0xF0) >> 4) + 1;
      int y = (VCOUNT % mosaicY);
      realX -= y * bg->dmx;
      realY -= y * bg->dmy;
   }

   int xxx = (realX >> 8);
   int yyy = (realY >> 8);

   for (int x = 0; x < 240; ++x)
   {
      if (xxx < 0 || yyy < 0 || xxx >= sizeX || yyy >= sizeY)
      {
         line2[x] = 0x80000000;
      }
      else
      {
         line2[x] = (READ16LE(&screenBase[yyy * sizeX + xxx]) | prio);
      }
      realX += bg->dx;
      realY += bg->dy;

      xxx = (realX >> 8);
      yyy = (realY >> 8);
   }

   if (bg->control & 0x40)
   {
      int mosaicX = (MOSAIC & 0xF) + 1;
      if (mosaicX > 1)
      {
         int m = 1;
         for (int i = 0; i < 239; ++i)
         {
            line2[i + 1] = line2[i];
            if (++m == mosaicX)
            {
               m = 1;
               ++i;
            }
         }
      }
   }
}

void gfxDrawRotScreen256(lcd_background_t* bg)
{
   uint16_t* palette = (uint16_t*)paletteRAM;
   uint8_t* screenBase = (DISPCNT & 0x0010) ? &vram[0xA000] : &vram[0x0000];
   int prio = ((bg->control & 3) << 25) + 0x1000000;
   int sizeX = 240;
   int sizeY = 160;

   int realX = bg->x_pos;
   int realY = bg->y_pos;

   if (bg->control & 0x40)
   {
      int mosaicY = ((MOSAIC & 0xF0) >> 4) + 1;
      int y = VCOUNT - (VCOUNT % mosaicY);
      realX = bg->x_ref + y * bg->dmx;
      realY = bg->y_ref + y * bg->dmy;
   }

   int xxx = (realX >> 8);
   int yyy = (realY >> 8);

   for (int x = 0; x < 240; ++x)
   {
      if (xxx < 0 || yyy < 0 || xxx >= sizeX || yyy >= sizeY)
      {
         line2[x] = 0x80000000;
      }
      else
      {
         uint8_t color = screenBase[yyy * 240 + xxx];

         line2[x] = color ? (READ16LE(&palette[color]) | prio) : 0x80000000;
      }
      realX += bg->dx;
      realY += bg->dy;

      xxx = (realX >> 8);
      yyy = (realY >> 8);
   }

   if (bg->control & 0x40)
   {
      int mosaicX = (MOSAIC & 0xF) + 1;
      if (mosaicX > 1)
      {
         int m = 1;
         for (int i = 0; i < 239; ++i)
         {
            line2[i + 1] = line2[i];
            if (++m == mosaicX)
            {
               m = 1;
               ++i;
            }
         }
      }
   }
}

void gfxDrawRotScreen16Bit160(lcd_background_t* bg)
{
   uint16_t* screenBase = (DISPCNT & 0x0010) ? (uint16_t*)&vram[0xa000] : (uint16_t*)&vram[0];
   int prio = ((bg->control & 3) << 25) + 0x1000000;
   int sizeX = 160;
   int sizeY = 128;

   int realX = bg->x_pos;
   int realY = bg->y_pos;

   if (bg->control & 0x40)
   {
      int mosaicY = ((MOSAIC & 0xF0) >> 4) + 1;
      int y = VCOUNT - (VCOUNT % mosaicY);
      realX = bg->x_ref + y * bg->dmx;
      realY = bg->y_ref + y * bg->dmy;
   }

   int xxx = (realX >> 8);
   int yyy = (realY >> 8);

   for (int x = 0; x < 240; ++x)
   {
      if (xxx < 0 || yyy < 0 || xxx >= sizeX || yyy >= sizeY)
      {
         line2[x] = 0x80000000;
      }
      else
      {
         line2[x] = (READ16LE(&screenBase[yyy * sizeX + xxx]) | prio);
      }
      realX += bg->dx;
      realY += bg->dy;

      xxx = (realX >> 8);
      yyy = (realY >> 8);
   }

   if (bg->control & 0x40)
   {
      int mosaicX = (MOSAIC & 0xF) + 1;
      if (mosaicX > 1)
      {
         int m = 1;
         for (int i = 0; i < 239; ++i)
         {
            line2[i + 1] = line2[i];
            if (++m == mosaicX)
            {
               m = 1;
               ++i;
            }
         }
      }
   }
}

#define OBJSHAPE (a0 >> 14)
#define OBJSIZE  (a1 >> 14)

static int objSizeMapX[4][4] = {
   {  8, 16,  8, 8 },
   { 16, 32,  8, 8 },
   { 32, 32, 16, 8 },
   { 64, 64, 32, 8 }
};

static int objSizeMapY[4][4] = {
   {  8,  8, 16, 8 },
   { 16,  8, 32, 8 },
   { 32, 16, 32, 8 },
   { 64, 32, 64, 8 }
};

void gfxDrawSprites()
{
   // lineOBJpix is used to keep track of the drawn OBJs
   // and to stop drawing them if the 'maximum number of OBJ per line'
   // has been reached.
   int lineOBJpix = (DISPCNT & 0x20) ? 954 : 1226;
   int m = 0;
   gfxClearArray(lineOBJ);
   if (layerEnable & 0x1000)
   {
      uint16_t* sprites = (uint16_t*)oam;
      uint16_t* spritePalette = &((uint16_t*)paletteRAM)[256];
      int mosaicY = ((MOSAIC & 0xF000) >> 12) + 1;
      int mosaicX = ((MOSAIC & 0xF00) >> 8) + 1;
      for (int x = 0; x < 128; ++x)
      {
         uint16_t a0 = READ16LE(sprites++);
         uint16_t a1 = READ16LE(sprites++);
         uint16_t a2 = READ16LE(sprites++);
         ++sprites;

         lineOBJpixleft[x] = lineOBJpix;

         lineOBJpix -= 2;
         if (lineOBJpix <= 0) {
            // make sure OBJwin, if used would stop as well where is should
            lineOBJpixleft[x + 1] = 0;
            return;
         }

         if ((a0 & 0x0c00) == 0x0c00)
            a0 &= 0xF3FF;

         int sizeX = objSizeMapX[OBJSIZE][OBJSHAPE];
         int sizeY = objSizeMapY[OBJSIZE][OBJSHAPE];

#ifdef SPRITE_DEBUG
         int maskX = sizeX - 1;
         int maskY = sizeY - 1;
#endif

         int sy = (a0 & 255);
         int sx = (a1 & 0x1FF);

         // computes ticks used by OBJ-WIN if OBJWIN is enabled
         if (((a0 & 0x0c00) == 0x0800) && (layerEnable & 0x8000))
         {
            if ((a0 & 0x0300) == 0x0300)
            {
               sizeX <<= 1;
               sizeY <<= 1;
            }
            if ((sy + sizeY) > 256)
               sy -= 256;
            if ((sx + sizeX) > 512)
               sx -= 512;
            if (sx < 0)
            {
               sizeX += sx;
               sx = 0;
            }
            else if ((sx + sizeX) > 240)
               sizeX = 240 - sx;
            if ((VCOUNT >= sy) && (VCOUNT < sy + sizeY) && (sx < 240))
            {
               if (a0 & 0x0100)
                  lineOBJpix -= 8 + 2 * sizeX;
               else
                  lineOBJpix -= sizeX - 2;
            }
            continue;
         }
         // else ignores OBJ-WIN if OBJWIN is disabled, and ignored disabled OBJ
         else if (((a0 & 0x0c00) == 0x0800) || ((a0 & 0x0300) == 0x0200))
            continue;

         if (a0 & 0x0100)
         {
            int fieldX = sizeX;
            int fieldY = sizeY;
            if (a0 & 0x0200)
            {
               fieldX <<= 1;
               fieldY <<= 1;
            }
            if ((sy + fieldY) > 256)
               sy -= 256;
            int t = VCOUNT - sy;
            if ((t >= 0) && (t < fieldY))
            {
               int startpix = 0;
               if ((sx + fieldX) > 512)
               {
                  startpix = 512 - sx;
               }
               if (lineOBJpix > 0)
               {
                  if ((sx < 240) || startpix)
                  {
                     lineOBJpix -= 8;
                     int rot = (a1 >> 9) & 0x1F;
                     uint16_t* OAM = (uint16_t*)oam;
                     int dx = READ16LE(&OAM[3 + (rot << 4)]);
                     if (dx & 0x8000)
                        dx |= 0xFFFF8000;
                     int dmx = READ16LE(&OAM[7 + (rot << 4)]);
                     if (dmx & 0x8000)
                        dmx |= 0xFFFF8000;
                     int dy = READ16LE(&OAM[11 + (rot << 4)]);
                     if (dy & 0x8000)
                        dy |= 0xFFFF8000;
                     int dmy = READ16LE(&OAM[15 + (rot << 4)]);
                     if (dmy & 0x8000)
                        dmy |= 0xFFFF8000;

                     if (a0 & 0x1000)
                     {
                        t -= (t % mosaicY);
                     }

                     int realX = ((sizeX) << 7) - (fieldX >> 1) * dx - (fieldY >> 1) * dmx + t * dmx;
                     int realY = ((sizeY) << 7) - (fieldX >> 1) * dy - (fieldY >> 1) * dmy + t * dmy;

                     uint32_t prio = (((a2 >> 10) & 3) << 25) | ((a0 & 0x0c00) << 6);

                     int c = (a2 & 0x3FF);
                     if ((DISPCNT & 7) > 2 && (c < 512))
                        continue;

                     if (a0 & 0x2000)
                     {
                        int inc = 32;
                        if (DISPCNT & 0x40)
                           inc = sizeX >> 2;
                        else
                           c &= 0x3FE;
                        for (int x = 0; x < fieldX; ++x)
                        {
                           if (x >= startpix)
                              lineOBJpix -= 2;
                           if (lineOBJpix < 0)
                              continue;
                           int xxx = realX >> 8;
                           int yyy = realY >> 8;

                           if (xxx < 0 || xxx >= sizeX || yyy < 0 || yyy >= sizeY || sx >= 240)
                           {
                           }
                           else
                           {
                              uint32_t color = vram[0x10000 + ((((c + (yyy >> 3) * inc) << 5) + ((yyy & 7) << 3) + ((xxx >> 3) << 6) + (xxx & 7)) & 0x7FFF)];
                              if ((color == 0) && (((prio >> 25) & 3) < ((lineOBJ[sx] >> 25) & 3)))
                              {
                                 lineOBJ[sx] = (lineOBJ[sx] & 0xF9FFFFFF) | prio;
                                 if ((a0 & 0x1000) && m)
                                    lineOBJ[sx] = (lineOBJ[sx - 1] & 0xF9FFFFFF) | prio;
                              }
                              else if (color && (prio < (lineOBJ[sx] & 0xFF000000)))
                              {
                                 lineOBJ[sx] = READ16LE(&spritePalette[color]) | prio;
                                 if ((a0 & 0x1000) && m)
                                    lineOBJ[sx] = (lineOBJ[sx - 1] & 0xF9FFFFFF) | prio;
                              }

                              if (a0 & 0x1000)
                              {
                                 if (++m == mosaicX)
                                    m = 0;
                              }
#ifdef SPRITE_DEBUG
                              if (t == 0 || t == maskY || x == 0 || x == maskX)
                                 lineOBJ[sx] = 0x001F;
#endif
                           }
                           sx = (sx + 1) & 511;
                           realX += dx;
                           realY += dy;
                        }
                     }
                     else
                     {
                        int inc = 32;
                        if (DISPCNT & 0x40)
                           inc = sizeX >> 3;
                        int palette = (a2 >> 8) & 0xF0;
                        for (int x = 0; x < fieldX; ++x)
                        {
                           if (x >= startpix)
                              lineOBJpix -= 2;
                           if (lineOBJpix < 0)
                              continue;
                           int xxx = realX >> 8;
                           int yyy = realY >> 8;
                           if (xxx < 0 || xxx >= sizeX || yyy < 0 || yyy >= sizeY || sx >= 240)
                           {
                           }
                           else
                           {
                              uint32_t color = vram[0x10000 + ((((c + (yyy >> 3) * inc) << 5) + ((yyy & 7) << 2) + ((xxx >> 3) << 5) + ((xxx & 7) >> 1)) & 0x7FFF)];
                              if (xxx & 1)
                                 color >>= 4;
                              else
                                 color &= 0x0F;

                              if ((color == 0) && (((prio >> 25) & 3) < ((lineOBJ[sx] >> 25) & 3)))
                              {
                                 lineOBJ[sx] = (lineOBJ[sx] & 0xF9FFFFFF) | prio;
                                 if ((a0 & 0x1000) && m)
                                    lineOBJ[sx] = (lineOBJ[sx - 1] & 0xF9FFFFFF) | prio;
                              }
                              else if (color && (prio < (lineOBJ[sx] & 0xFF000000)))
                              {
                                 lineOBJ[sx] = READ16LE(&spritePalette[palette + color]) | prio;
                                 if ((a0 & 0x1000) && m)
                                    lineOBJ[sx] = (lineOBJ[sx - 1] & 0xF9FFFFFF) | prio;
                              }
                           }
                           if ((a0 & 0x1000) && m)
                           {
                              if (++m == mosaicX)
                                 m = 0;
                           }

#ifdef SPRITE_DEBUG
                           if (t == 0 || t == maskY || x == 0 || x == maskX)
                              lineOBJ[sx] = 0x001F;
#endif
                           sx = (sx + 1) & 511;
                           realX += dx;
                           realY += dy;
                        }
                     }
                  }
               }
            }
         }
         else
         {
            if (sy + sizeY > 256)
               sy -= 256;
            int t = VCOUNT - sy;
            if ((t >= 0) && (t < sizeY))
            {
               int startpix = 0;
               if ((sx + sizeX) > 512)
               {
                  startpix = 512 - sx;
               }
               if ((sx < 240) || startpix)
               {
                  lineOBJpix += 2;
                  if (a0 & 0x2000)
                  {
                     if (a1 & 0x2000)
                        t = sizeY - t - 1;
                     int c = (a2 & 0x3FF);
                     if ((DISPCNT & 7) > 2 && (c < 512))
                        continue;

                     int inc = 32;
                     if (DISPCNT & 0x40)
                     {
                        inc = sizeX >> 2;
                     }
                     else
                     {
                        c &= 0x3FE;
                     }
                     int xxx = 0;
                     if (a1 & 0x1000)
                        xxx = sizeX - 1;

                     if (a0 & 0x1000)
                     {
                        t -= (t % mosaicY);
                     }

                     int address = 0x10000 + ((((c + (t >> 3) * inc) << 5) + ((t & 7) << 3) + ((xxx >> 3) << 6) + (xxx & 7)) & 0x7FFF);

                     if (a1 & 0x1000)
                        xxx = 7;
                     uint32_t prio = (((a2 >> 10) & 3) << 25) | ((a0 & 0x0c00) << 6);

                     for (int xx = 0; xx < sizeX; ++xx)
                     {
                        if (xx >= startpix)
                           --lineOBJpix;
                        if (lineOBJpix < 0)
                           continue;
                        if (sx < 240)
                        {
                           uint8_t color = vram[address];
                           if ((color == 0) && (((prio >> 25) & 3) < ((lineOBJ[sx] >> 25) & 3)))
                           {
                              lineOBJ[sx] = (lineOBJ[sx] & 0xF9FFFFFF) | prio;
                              if ((a0 & 0x1000) && m)
                              {
                                 lineOBJ[sx] = (lineOBJ[sx - 1] & 0xF9FFFFFF) | prio;
                              }
                           }
                           else if ((color) && (prio < (lineOBJ[sx] & 0xFF000000)))
                           {
                              lineOBJ[sx] = READ16LE(&spritePalette[color]) | prio;
                              if ((a0 & 0x1000) && m)
                              {
                                 lineOBJ[sx] = (lineOBJ[sx - 1] & 0xF9FFFFFF) | prio;
                              }
                           }

                           if (a0 & 0x1000)
                           {
                              if (++m == mosaicX)
                              {
                                 m = 0;
                              }
                           }

#ifdef SPRITE_DEBUG
                           if (t == 0 || t == maskY || xx == 0 || xx == maskX)
                              lineOBJ[sx] = 0x001F;
#endif
                        }

                        sx = (sx + 1) & 511;
                        if (a1 & 0x1000)
                        {
                           --address;
                           if (--xxx == -1)
                           {
                              address -= 56;
                              xxx = 7;
                           }
                           if (address < 0x10000)
                           {
                              address += 0x8000;
                           }
                        }
                        else
                        {
                           ++address;
                           if (++xxx == 8)
                           {
                              address += 56;
                              xxx = 0;
                           }
                           if (address > 0x17fff)
                           {
                              address -= 0x8000;
                           }
                        }
                     }
                  }
                  else
                  {
                     if (a1 & 0x2000)
                     {
                        t = sizeY - t - 1;
                     }
                     int c = (a2 & 0x3FF);
                     if ((DISPCNT & 7) > 2 && (c < 512))
                        continue;

                     int inc = 32;
                     if (DISPCNT & 0x40)
                     {
                        inc = sizeX >> 3;
                     }
                     int xxx = 0;
                     if (a1 & 0x1000)
                     {
                        xxx = sizeX - 1;
                     }

                     if (a0 & 0x1000)
                     {
                        t -= (t % mosaicY);
                     }

                     int address = 0x10000 + ((((c + (t >> 3) * inc) << 5) + ((t & 7) << 2) + ((xxx >> 3) << 5) + ((xxx & 7) >> 1)) & 0x7FFF);
                     uint32_t prio = (((a2 >> 10) & 3) << 25) | ((a0 & 0x0c00) << 6);
                     int palette = (a2 >> 8) & 0xF0;
                     if (a1 & 0x1000)
                     {
                        xxx = 7;
                        for (int xx = sizeX - 1; xx >= 0;
                             --xx)
                        {
                           if (xx >= startpix)
                              --lineOBJpix;
                           if (lineOBJpix < 0)
                              continue;
                           if (sx < 240)
                           {
                              uint8_t color = vram[address];
                              if (xx & 1)
                              {
                                 color = (color >> 4);
                              }
                              else
                              {
                                 color &= 0x0F;
                              }

                              if ((color == 0) && (((prio >> 25) & 3) < ((lineOBJ[sx] >> 25) & 3)))
                              {
                                 lineOBJ[sx] = (lineOBJ[sx] & 0xF9FFFFFF) | prio;
                                 if ((a0 & 0x1000) && m)
                                    lineOBJ[sx] = (lineOBJ[sx - 1] & 0xF9FFFFFF) | prio;
                              }
                              else if ((color) && (prio < (lineOBJ[sx] & 0xFF000000)))
                              {
                                 lineOBJ[sx] = READ16LE(&spritePalette[palette + color]) | prio;
                                 if ((a0 & 0x1000) && m)
                                    lineOBJ[sx] = (lineOBJ[sx - 1] & 0xF9FFFFFF) | prio;
                              }
                           }
                           if (a0 & 0x1000)
                           {
                              if (++m == mosaicX)
                              {
                                 m = 0;
                              }
                           }
#ifdef SPRITE_DEBUG
                           if (t == 0 || t == maskY || xx == 0 || xx == maskX)
                              lineOBJ[sx] = 0x001F;
#endif
                           sx = (sx + 1) & 511;
                           if (!(xx & 1))
                           {
                              --address;
                           }
                           if (--xxx == -1)
                           {
                              xxx = 7;
                              address -= 28;
                           }
                           if (address < 0x10000)
                           {
                              address += 0x8000;
                           }
                        }
                     }
                     else
                     {
                        for (int xx = 0; xx < sizeX; ++xx)
                        {
                           if (xx >= startpix)
                           {
                              --lineOBJpix;
                           }
                           if (lineOBJpix < 0)
                           {
                              continue;
                           }
                           if (sx < 240)
                           {
                              uint8_t color = vram[address];
                              if (xx & 1)
                              {
                                 color = (color >> 4);
                              }
                              else
                                 color &= 0x0F;

                              if ((color == 0) && (((prio >> 25) & 3) < ((lineOBJ[sx] >> 25) & 3)))
                              {
                                 lineOBJ[sx] = (lineOBJ[sx] & 0xF9FFFFFF) | prio;
                                 if ((a0 & 0x1000) && m)
                                 {
                                    lineOBJ[sx] = (lineOBJ[sx - 1] & 0xF9FFFFFF) | prio;
                                 }
                              }
                              else if (color && (prio < (lineOBJ[sx] & 0xFF000000)))
                              {
                                 lineOBJ[sx] = READ16LE(&spritePalette[palette + color]) | prio;
                                 if ((a0 & 0x1000) && m)
                                 {
                                    lineOBJ[sx] = (lineOBJ[sx - 1] & 0xF9FFFFFF) | prio;
                                 }
                              }
                           }
                           if (a0 & 0x1000)
                           {
                              if (++m == mosaicX)
                                 m = 0;
                           }
#ifdef SPRITE_DEBUG
                           if (t == 0 || t == maskY || xx == 0 || xx == maskX)
                              lineOBJ[sx] = 0x001F;
#endif
                           sx = (sx + 1) & 511;
                           if (xx & 1)
                           {
                              ++address;
                           }
                           if (++xxx == 8)
                           {
                              address += 28;
                              xxx = 0;
                           }
                           if (address > 0x17fff)
                           {
                              address -= 0x8000;
                           }
                        }
                     }
                  }
               }
            }
         }
      }
   }
}

void gfxDrawOBJWin()
{
   gfxClearArray(lineOBJWin);
   if ((layerEnable & 0x9000) == 0x9000)
   {
      uint16_t* sprites = (uint16_t*)oam;
      for (int x = 0; x < 128; ++x)
      {
         int lineOBJpix = lineOBJpixleft[x];
         uint16_t a0 = READ16LE(sprites++);
         uint16_t a1 = READ16LE(sprites++);
         uint16_t a2 = READ16LE(sprites++);
         ++sprites;

         if (lineOBJpix <= 0)
            return;

         // ignores non OBJ-WIN and disabled OBJ-WIN
         if (((a0 & 0x0c00) != 0x0800) || ((a0 & 0x0300) == 0x0200))
            continue;

         if ((a0 & 0x0c00) == 0x0c00)
            a0 &= 0xF3FF;

         int sizeX = objSizeMapX[OBJSIZE][OBJSHAPE];
         int sizeY = objSizeMapY[OBJSIZE][OBJSHAPE];

         int sy = (a0 & 255);

         if (a0 & 0x0100)
         {
            int fieldX = sizeX;
            int fieldY = sizeY;
            if (a0 & 0x0200)
            {
               fieldX <<= 1;
               fieldY <<= 1;
            }
            if ((sy + fieldY) > 256)
               sy -= 256;
            int t = VCOUNT - sy;
            if ((t >= 0) && (t < fieldY))
            {
               int sx = (a1 & 0x1FF);
               int startpix = 0;
               if ((sx + fieldX) > 512)
               {
                  startpix = 512 - sx;
               }
               if ((sx < 240) || startpix)
               {
                  lineOBJpix -= 8;
                  int rot = (a1 >> 9) & 0x1F;
                  uint16_t* OAM = (uint16_t*)oam;
                  int dx = READ16LE(&OAM[3 + (rot << 4)]);
                  if (dx & 0x8000)
                     dx |= 0xFFFF8000;
                  int dmx = READ16LE(&OAM[7 + (rot << 4)]);
                  if (dmx & 0x8000)
                     dmx |= 0xFFFF8000;
                  int dy = READ16LE(&OAM[11 + (rot << 4)]);
                  if (dy & 0x8000)
                     dy |= 0xFFFF8000;
                  int dmy = READ16LE(&OAM[15 + (rot << 4)]);
                  if (dmy & 0x8000)
                     dmy |= 0xFFFF8000;

                  int realX = ((sizeX) << 7) - (fieldX >> 1) * dx - (fieldY >> 1) * dmx + t * dmx;
                  int realY = ((sizeY) << 7) - (fieldX >> 1) * dy - (fieldY >> 1) * dmy + t * dmy;

                  if (a0 & 0x2000)
                  {
                     int c = (a2 & 0x3FF);
                     if ((DISPCNT & 7) > 2 && (c < 512))
                        continue;
                     int inc = 32;
                     if (DISPCNT & 0x40)
                        inc = sizeX >> 2;
                     else
                        c &= 0x3FE;
                     for (int x = 0; x < fieldX; ++x)
                     {
                        if (x >= startpix)
                           lineOBJpix -= 2;
                        if (lineOBJpix < 0)
                           continue;
                        int xxx = realX >> 8;
                        int yyy = realY >> 8;

                        if (xxx < 0 || xxx >= sizeX || yyy < 0 || yyy >= sizeY || sx >= 240)
                        {
                        }
                        else
                        {
                           uint32_t color = vram
                               [0x10000 + ((((c + (yyy >> 3) * inc) << 5) + ((yyy & 7) << 3) + ((xxx >> 3) << 6) + (xxx & 7)) & 0x7fff)];
                           if (color)
                           {
                              lineOBJWin[sx] = 1;
                           }
                        }
                        sx = (sx + 1) & 511;
                        realX += dx;
                        realY += dy;
                     }
                  }
                  else
                  {
                     int c = (a2 & 0x3FF);
                     if ((DISPCNT & 7) > 2 && (c < 512))
                        continue;

                     int inc = 32;
                     if (DISPCNT & 0x40)
                        inc = sizeX >> 3;
                     for (int x = 0; x < fieldX; ++x)
                     {
                        if (x >= startpix)
                           lineOBJpix -= 2;
                        if (lineOBJpix < 0)
                           continue;
                        int xxx = realX >> 8;
                        int yyy = realY >> 8;

                        if (xxx < 0 || xxx >= sizeX || yyy < 0 || yyy >= sizeY || sx >= 240)
                        {
                        }
                        else
                        {
                           uint32_t color = vram
                               [0x10000 + ((((c + (yyy >> 3) * inc) << 5) + ((yyy & 7) << 2) + ((xxx >> 3) << 5) + ((xxx & 7) >> 1)) & 0x7fff)];
                           if (xxx & 1)
                              color >>= 4;
                           else
                              color &= 0x0F;

                           if (color)
                           {
                              lineOBJWin[sx] = 1;
                           }
                        }
                        sx = (sx + 1) & 511;
                        realX += dx;
                        realY += dy;
                     }
                  }
               }
            }
         }
         else
         {
            if ((sy + sizeY) > 256)
               sy -= 256;
            int t = VCOUNT - sy;
            if ((t >= 0) && (t < sizeY))
            {
               int sx = (a1 & 0x1FF);
               int startpix = 0;
               if ((sx + sizeX) > 512)
               {
                  startpix = 512 - sx;
               }
               if ((sx < 240) || startpix)
               {
                  lineOBJpix += 2;
                  if (a0 & 0x2000)
                  {
                     if (a1 & 0x2000)
                        t = sizeY - t - 1;
                     int c = (a2 & 0x3FF);
                     if ((DISPCNT & 7) > 2 && (c < 512))
                        continue;

                     int inc = 32;
                     if (DISPCNT & 0x40)
                     {
                        inc = sizeX >> 2;
                     }
                     else
                     {
                        c &= 0x3FE;
                     }
                     int xxx = 0;
                     if (a1 & 0x1000)
                        xxx = sizeX - 1;
                     int address = 0x10000 + ((((c + (t >> 3) * inc) << 5) + ((t & 7) << 3) + ((xxx >> 3) << 6) + (xxx & 7)) & 0x7fff);
                     if (a1 & 0x1000)
                        xxx = 7;
                     for (int xx = 0; xx < sizeX; ++xx)
                     {
                        if (xx >= startpix)
                           --lineOBJpix;
                        if (lineOBJpix < 0)
                           continue;
                        if (sx < 240)
                        {
                           uint8_t color = vram[address];
                           if (color)
                           {
                              lineOBJWin[sx] = 1;
                           }
                        }

                        sx = (sx + 1) & 511;
                        if (a1 & 0x1000)
                        {
                           --address;
                           if (--xxx == -1)
                           {
                              address -= 56;
                              xxx = 7;
                           }
                           if (address < 0x10000)
                              address += 0x8000;
                        }
                        else
                        {
                           ++address;
                           if (++xxx == 8)
                           {
                              address += 56;
                              xxx = 0;
                           }
                           if (address > 0x17fff)
                              address -= 0x8000;
                        }
                     }
                  }
                  else
                  {
                     if (a1 & 0x2000)
                        t = sizeY - t - 1;
                     int c = (a2 & 0x3FF);
                     if ((DISPCNT & 7) > 2 && (c < 512))
                        continue;

                     int inc = 32;
                     if (DISPCNT & 0x40)
                     {
                        inc = sizeX >> 3;
                     }
                     int xxx = 0;
                     if (a1 & 0x1000)
                        xxx = sizeX - 1;
                     int address = 0x10000 + ((((c + (t >> 3) * inc) << 5) + ((t & 7) << 2) + ((xxx >> 3) << 5) + ((xxx & 7) >> 1)) & 0x7fff);

                     if (a1 & 0x1000)
                     {
                        xxx = 7;
                        for (int xx = sizeX - 1; xx >= 0;
                             --xx)
                        {
                           if (xx >= startpix)
                              --lineOBJpix;
                           if (lineOBJpix < 0)
                              continue;
                           if (sx < 240)
                           {
                              uint8_t color = vram[address];
                              if (xx & 1)
                              {
                                 color = (color >> 4);
                              }
                              else
                                 color &= 0x0F;

                              if (color)
                              {
                                 lineOBJWin
                                     [sx]
                                     = 1;
                              }
                           }
                           sx = (sx + 1) & 511;
                           if (!(xx & 1))
                              --address;
                           if (--xxx == -1)
                           {
                              xxx = 7;
                              address -= 28;
                           }
                           if (address < 0x10000)
                              address += 0x8000;
                        }
                     }
                     else
                     {
                        for (int xx = 0; xx < sizeX; ++xx)
                        {
                           if (xx >= startpix)
                              --lineOBJpix;
                           if (lineOBJpix < 0)
                              continue;
                           if (sx < 240)
                           {
                              uint8_t color = vram[address];
                              if (xx & 1)
                              {
                                 color = (color >> 4);
                              }
                              else
                                 color &= 0x0F;

                              if (color)
                              {
                                 lineOBJWin
                                     [sx]
                                     = 1;
                              }
                           }
                           sx = (sx + 1) & 511;
                           if (xx & 1)
                              ++address;
                           if (++xxx == 8)
                           {
                              address += 28;
                              xxx = 0;
                           }
                           if (address > 0x17fff)
                              address -= 0x8000;
                        }
                     }
                  }
               }
            }
         }
      }
   }
}

void LCDUpdateBGCNT(lcd_background_t* bg, uint16_t value)
{
   bg->control = value;
}

void LCDUpdateBGHOFS(lcd_background_t* bg, uint16_t value)
{
   bg->h_offset = value;
}

void LCDUpdateBGVOFS(lcd_background_t* bg, uint16_t value)
{
   bg->v_offset = value;
}

void LCDUpdateBGPA(lcd_background_t* bg, uint16_t value)
{
   bg->dx = value & 0x7FFF;
   if (value & 0x8000)
      bg->dx |= 0xFFFF8000;
}

void LCDUpdateBGPB(lcd_background_t* bg, uint16_t value)
{
   bg->dmx = value & 0x7FFF;
   if (value & 0x8000)
      bg->dmx |= 0xFFFF8000;
}

void LCDUpdateBGPC(lcd_background_t* bg, uint16_t value)
{
   bg->dy = value & 0x7FFF;
   if (value & 0x8000)
      bg->dy |= 0xFFFF8000;
}

void LCDUpdateBGPD(lcd_background_t* bg, uint16_t value)
{
   bg->dmy = value & 0x7FFF;
   if (value & 0x8000)
      bg->dmy |= 0xFFFF8000;
}

void LCDUpdateBGX_L(lcd_background_t* bg, uint16_t value)
{
   bg->x_ref &= 0xFFFF0000;
   bg->x_ref |= value;
   bg->x_pos = bg->x_ref;
}

void LCDUpdateBGX_H(lcd_background_t* bg, uint16_t value)
{
   bg->x_ref &= 0x0000FFFF;
   bg->x_ref |= (value << 16);
   if (value & 0x0800)
      bg->x_ref |= 0xF8000000;
   bg->x_pos = bg->x_ref;
}

void LCDUpdateBGY_L(lcd_background_t* bg, uint16_t value)
{
   bg->y_ref &= 0xFFFF0000;
   bg->y_ref |= value;
   bg->y_pos = bg->y_ref;
}
void LCDUpdateBGY_H(lcd_background_t* bg, uint16_t value)
{
   bg->y_ref &= 0x0000FFFF;
   bg->y_ref |= (value << 16);
   if (value & 0x0800)
      bg->y_ref |= 0xF8000000;
   bg->y_pos = bg->y_ref;
}

void LCDUpdateWindow0(void)
{
    int x00 = WIN0H >> 8;
    int x01 = WIN0H & 255;

    if (x00 <= x01) {
        for (int i = 0; i < 240; ++i) {
            gfxInWin0[i] = (i >= x00 && i < x01);
        }
    } else {
        for (int i = 0; i < 240; ++i) {
            gfxInWin0[i] = (i >= x00 || i < x01);
        }
    }
}

void LCDUpdateWindow1(void)
{
    int x00 = WIN1H >> 8;
    int x01 = WIN1H & 255;

    if (x00 <= x01) {
        for (int i = 0; i < 240; ++i) {
            gfxInWin1[i] = (i >= x00 && i < x01);
        }
    } else {
        for (int i = 0; i < 240; ++i) {
            gfxInWin1[i] = (i >= x00 || i < x01);
        }
    }
}

void LCDUpdateBGRef(int BG)
{
   uint32_t x_value = 0;
   uint32_t y_value = 0;
   switch (BG)
   {
   case 2:
      x_value = READ32LE((uint32_t*)&ioMem[0x28]);
      y_value = READ32LE((uint32_t*)&ioMem[0x2C]);
      break;

   case 3:
      x_value = READ32LE((uint32_t*)&ioMem[0x38]);
      y_value = READ32LE((uint32_t*)&ioMem[0x3C]);
      break;
   }

   lcd_bg[BG].x_ref = x_value & 0x07FFFFFF;
   if (x_value & 0x08000000)
      lcd_bg[BG].x_ref |= 0xF8000000;
   lcd_bg[BG].x_pos = lcd_bg[BG].x_ref;

   lcd_bg[BG].y_ref = y_value & 0x07FFFFFF;
   if (y_value & 0x08000000)
      lcd_bg[BG].y_ref |= 0xF8000000;
   lcd_bg[BG].y_pos = lcd_bg[BG].y_ref;
}

void LCDResetBGParams()
{
   for (int i = 0; i < 4; ++i)
   {
      lcd_background_t* bg = &lcd_bg[i];
      bg->control = 0;
      bg->h_offset = 0;
      bg->v_offset = 0;
      bg->dx = 0x100;
      bg->dmx = 0;
      bg->dy = 0;
      bg->dmy = 0x100;
      bg->x_ref = 0;
      bg->y_ref = 0;
      bg->x_pos = 0;
      bg->y_pos = 0;
   }
}

void LCDLoadStateBGParams()
{
   LCDUpdateBGCNT(&lcd_bg[0], BG0CNT);
   LCDUpdateBGCNT(&lcd_bg[1], BG1CNT);
   LCDUpdateBGCNT(&lcd_bg[2], BG2CNT);
   LCDUpdateBGCNT(&lcd_bg[3], BG3CNT);
   LCDUpdateBGHOFS(&lcd_bg[0], BG0HOFS);
   LCDUpdateBGVOFS(&lcd_bg[0], BG0VOFS);
   LCDUpdateBGHOFS(&lcd_bg[1], BG1HOFS);
   LCDUpdateBGVOFS(&lcd_bg[1], BG1VOFS);
   LCDUpdateBGHOFS(&lcd_bg[2], BG2HOFS);
   LCDUpdateBGVOFS(&lcd_bg[2], BG2VOFS);
   LCDUpdateBGHOFS(&lcd_bg[3], BG3HOFS);
   LCDUpdateBGVOFS(&lcd_bg[3], BG3VOFS);

   LCDUpdateBGPA(&lcd_bg[2], BG2PA);
   LCDUpdateBGPB(&lcd_bg[2], BG2PB);
   LCDUpdateBGPC(&lcd_bg[2], BG2PC);
   LCDUpdateBGPD(&lcd_bg[2], BG2PD);
   LCDUpdateBGPA(&lcd_bg[3], BG3PA);
   LCDUpdateBGPB(&lcd_bg[3], BG3PB);
   LCDUpdateBGPC(&lcd_bg[3], BG3PC);
   LCDUpdateBGPD(&lcd_bg[3], BG3PD);
   LCDUpdateBGRef(2);
   LCDUpdateBGRef(3);
}
