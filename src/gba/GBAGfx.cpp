#include "GBAGfx.h"
#include "../System.h"
#include <string.h>

int coeff[32] = {
   0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
   16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16
};

uint32_t lineBG[4][240];
uint32_t lineOBJ[240];
uint32_t lineOBJWin[240];
bool gfxInWin0[240];
bool gfxInWin1[240];
int lineOBJpixleft[128];

gba_lcd_t lcd;

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
struct TileLine
{
   uint32_t pixels[8];
};

typedef const TileLine (*TileReader)(const uint16_t*, const int, const uint8_t*, uint16_t*, const uint32_t);

static inline void gfxDrawPixel(uint32_t* dest, const uint8_t color, const uint16_t* palette, const uint32_t prio)
{
   *dest = color ? (READ16LE(&palette[color]) | prio) : 0x80000000;
}

#define BIT(x) (1 << (x))
#define TILENUM(x) ((x) & 0x03FF)
#define TILEPALETTE(x) (((x) >> 12) & 0x000F)
#define HFLIP BIT(10)
#define VFLIP BIT(11)

inline const TileLine gfxReadTile(const uint16_t* screenSource, const int yyy, const uint8_t* charBase, uint16_t* palette, const uint32_t prio)
{
   uint16_t tile = READ16LE(screenSource);
   int tileY = yyy & 7;
   if (tile & VFLIP)
      tileY = 7 - tileY;
   TileLine tileLine;

   const uint8_t* tileBase = &charBase[TILENUM(tile) * 64 + tileY * 8];

   if (!(tile & HFLIP))
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
   uint16_t tile = READ16LE(screenSource);
   int tileY = yyy & 7;
   if (tile & VFLIP)
      tileY = 7 - tileY;
   palette += TILEPALETTE(tile) * 16;
   TileLine tileLine;

   const uint8_t* tileBase = (uint8_t*)&charBase[TILENUM(tile) * 32 + tileY * 4];

   if (!(tile & HFLIP))
   {
      gfxDrawPixel(&tileLine.pixels[0], (tileBase[0] >> 0) & 0x0F, palette, prio);
      gfxDrawPixel(&tileLine.pixels[1], (tileBase[0] >> 4) & 0x0F, palette, prio);
      gfxDrawPixel(&tileLine.pixels[2], (tileBase[1] >> 0) & 0x0F, palette, prio);
      gfxDrawPixel(&tileLine.pixels[3], (tileBase[1] >> 4) & 0x0F, palette, prio);
      gfxDrawPixel(&tileLine.pixels[4], (tileBase[2] >> 0) & 0x0F, palette, prio);
      gfxDrawPixel(&tileLine.pixels[5], (tileBase[2] >> 4) & 0x0F, palette, prio);
      gfxDrawPixel(&tileLine.pixels[6], (tileBase[3] >> 0) & 0x0F, palette, prio);
      gfxDrawPixel(&tileLine.pixels[7], (tileBase[3] >> 4) & 0x0F, palette, prio);
   }
   else
   {
      gfxDrawPixel(&tileLine.pixels[0], (tileBase[3] >> 4) & 0x0F, palette, prio);
      gfxDrawPixel(&tileLine.pixels[1], (tileBase[3] >> 0) & 0x0F, palette, prio);
      gfxDrawPixel(&tileLine.pixels[2], (tileBase[2] >> 4) & 0x0F, palette, prio);
      gfxDrawPixel(&tileLine.pixels[3], (tileBase[2] >> 0) & 0x0F, palette, prio);
      gfxDrawPixel(&tileLine.pixels[4], (tileBase[1] >> 4) & 0x0F, palette, prio);
      gfxDrawPixel(&tileLine.pixels[5], (tileBase[1] >> 0) & 0x0F, palette, prio);
      gfxDrawPixel(&tileLine.pixels[6], (tileBase[0] >> 4) & 0x0F, palette, prio);
      gfxDrawPixel(&tileLine.pixels[7], (tileBase[0] >> 0) & 0x0F, palette, prio);
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
static void gfxDrawTextScreen(int layer)
{
   uint16_t *palette = (uint16_t*)paletteRAM;
   uint8_t* charBase = &vram[lcd.bg[layer].char_base];
   uint16_t* screenBase = (uint16_t*)&vram[lcd.bg[layer].screen_base];
   uint32_t prio = lcd.bg[layer].priority;

   int sizeX = textModeSizeMapX[lcd.bg[layer].screen_size];
   int sizeY = textModeSizeMapY[lcd.bg[layer].screen_size];

   int maskX = sizeX - 1;
   int maskY = sizeY - 1;

   bool mosaicOn = (lcd.bg[layer].mosaic) ? true : false;

   int xxx = lcd.bg[layer].hofs & maskX;
   int yyy = (lcd.bg[layer].vofs + lcd.vcount) & maskY;
   int mosaicX = (lcd.mosaic & 0x000F) + 1;
   int mosaicY = ((lcd.mosaic & 0x00F0) >> 4) + 1;

   if (mosaicOn)
   {
      if ((lcd.vcount % mosaicY) != 0)
      {
         mosaicY = lcd.vcount - (lcd.vcount % mosaicY);
         yyy = (lcd.bg[layer].vofs + mosaicY) & maskY;
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
      gfxDrawTileClipped(readTile(screenSource, yyy, charBase, palette, prio), &lineBG[layer][x], firstTileX, 8 - firstTileX);
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
      gfxDrawTile(readTile(screenSource, yyy, charBase, palette, prio), &lineBG[layer][x]);
      ++screenSource;
      xxx += 8;
      x += 8;

      if (xxx == 256 && sizeX > 256)
      {
         screenSource = screenBase + 0x400 + yshift;
         continue;
      }
      if (xxx >= sizeX)
      {
         xxx = 0;
         screenSource = screenBase + yshift;
      }
   }

   // Last tile, if clipped
   if (firstTileX)
   {
      gfxDrawTileClipped(readTile(screenSource, yyy, charBase, palette, prio), &lineBG[layer][x], 0, firstTileX);
   }

   if (mosaicOn)
   {
      if (mosaicX > 1)
      {
         int m = 1;
         for (int i = 0; i < 239; ++i)
         {
            lineBG[layer][i + 1] = lineBG[layer][i];
            if (++m == mosaicX)
            {
               m = 1;
               ++i;
            }
         }
      }
   }
}

void gfxDrawTextScreen(int layer)
{
   if ((layerEnable & (0x0100 << layer)) == 0)
      return;
   if (lcd.bg[layer].palette256) // 1 pal / 256 col
      gfxDrawTextScreen<gfxReadTile>(layer);
   else // 16 pal / 16 col
      gfxDrawTextScreen<gfxReadTilePal>(layer);
}

#else // TILED_RENDERING

void gfxDrawTextScreen(int layer)
{
   if ((layerEnable & (0x0100 << layer)) == 0)
      return;

   uint16_t* palette = (uint16_t*)paletteRAM;
   uint8_t* charBase = &vram[lcd.bg[layer].char_base];
   uint16_t* screenBase = (uint16_t*)&vram[lcd.bg[layer].screen_base];
   uint32_t prio = lcd.bg[layer].priority;

   int sizeX = textModeSizeMapX[lcd.bg[layer].screen_size];
   int sizeY = textModeSizeMapY[lcd.bg[layer].screen_size];

   int maskX = sizeX - 1;
   int maskY = sizeY - 1;

   int xxx = lcd.bg[layer].hofs & maskX;
   int yyy = (lcd.bg[layer].vofs + lcd.vcount) & maskY;
   int mosaicX = (lcd.mosaic & 0x000F) + 1;
   int mosaicY = ((lcd.mosaic & 0x00F0) >> 4) + 1;

   if (lcd.bg[layer].mosaic)
   {
      if ((lcd.vcount % mosaicY) != 0)
      {
         mosaicY = lcd.vcount - (lcd.vcount % mosaicY);
         yyy = (lcd.bg[layer].vofs + mosaicY) & maskY;
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
   if (lcd.bg[layer].overflow)
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

         lineBG[layer][x] = color ? (READ16LE(&palette[color]) | prio) : 0x80000000;

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
         lineBG[layer][x] = color ? (READ16LE(&palette[pal + color]) | prio) : 0x80000000;

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
   if (lcd.bg[layer].mosaic)
   {
      if (mosaicX > 1)
      {
         int m = 1;
         for (int i = 0; i < 239; i++)
         {
            lineBG[layer][i + 1] = lineBG[layer][i];
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

void gfxDrawRotScreen(int layer)
{
   if ((layerEnable & (0x0100 << layer)) == 0)
      return;

   uint16_t* palette = (uint16_t*)paletteRAM;
   uint8_t* charBase = &vram[lcd.bg[layer].char_base];
   uint8_t* screenBase = (uint8_t*)&vram[lcd.bg[layer].screen_base];
   int prio = lcd.bg[layer].priority;

   int sizeX = rotationModeSizeMap[lcd.bg[layer].screen_size];
   int sizeY = rotationModeSizeMap[lcd.bg[layer].screen_size];

   int maskX = sizeX - 1;
   int maskY = sizeY - 1;

   int yshift = lcd.bg[layer].screen_size + 4;

   int realX = lcd.bg[layer].x_pos;
   int realY = lcd.bg[layer].y_pos;

   if (lcd.bg[layer].mosaic)
   {
      int mosaicY = ((lcd.mosaic & 0xF0) >> 4) + 1;
      int y = (lcd.vcount % mosaicY);
      realX -= y * lcd.bg[layer].dmx;
      realY -= y * lcd.bg[layer].dmy;
   }

   gfxClearArray(lineBG[layer]);
   if (lcd.bg[layer].overflow) // Wrap around
   {
      if (lcd.bg[layer].dx > 0 && lcd.bg[layer].dy == 0) // Common subcase: no rotation or flipping
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
               lineBG[layer][x] = (READ16LE(&palette[color]) | prio);

            realX += lcd.bg[layer].dx;
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
               lineBG[layer][x] = (READ16LE(&palette[color]) | prio);

            realX += lcd.bg[layer].dx;
            realY += lcd.bg[layer].dy;
         }
      }
   }
   else // Culling
   {
      if (lcd.bg[layer].dx > 0 && lcd.bg[layer].dy == 0) // Common subcase: no rotation or flipping
      {
         int yyy = (realY >> 8);

         if (yyy < 0 || yyy >= sizeY)
            goto skipLine;

         int x0 = MAX<int32_t>(0, (int32_t)(+(-realX + lcd.bg[layer].dx - 1)) / lcd.bg[layer].dx);
         int x1 = MIN<int32_t>(240, (int32_t)((sizeX << 8) + (-realX + lcd.bg[layer].dx - 1)) / lcd.bg[layer].dx);

         if (x1 < 0)
            goto skipLine;

         int yyyshift = (yyy >> 3) << yshift;
         int tileY = yyy & 7;
         int tileYshift = (tileY << 3);

         realX += lcd.bg[layer].dx * x0;

         for (int x = x0; x < x1; ++x)
         {
            int xxx = (realX >> 8);

            int tile = screenBase[(xxx >> 3) | yyyshift];
            int tileX = (xxx & 7);

            uint8_t color = charBase[(tile << 6) | tileYshift | tileX];

            if (color)
               lineBG[layer][x] = (READ16LE(&palette[color]) | prio);

            realX += lcd.bg[layer].dx;
         }
      }
      else
      {
         for (int x = 0; x < 240; ++x)
         {
            int xxx = (realX >> 8);
            int yyy = (realY >> 8);

            realX += lcd.bg[layer].dx;
            realY += lcd.bg[layer].dy;

            if (xxx < 0 || yyy < 0 || xxx >= sizeX || yyy >= sizeY)
               continue;
            int tile = screenBase[(xxx >> 3) + ((yyy >> 3) << yshift)];

            int tileX = (xxx & 7);
            int tileY = yyy & 7;

            uint8_t color = charBase[(tile << 6) + (tileY << 3) + tileX];

            if (color)
               lineBG[layer][x] = (READ16LE(&palette[color]) | prio);
         }
      }
   }

skipLine:

   if (lcd.bg[layer].mosaic)
   {
      int mosaicX = (lcd.mosaic & 0xF) + 1;
      if (mosaicX > 1)
      {
         int m = 1;
         for (int i = 0; i < 239; ++i)
         {
            lineBG[layer][i + 1] = lineBG[layer][i];
            if (++m == mosaicX)
            {
               m = 1;
               ++i;
            }
         }
      }
   }
}

void gfxDrawRotScreen16Bit()
{
   if ((layerEnable & 0x0400) == 0)
      return;

   uint16_t* screenBase = (uint16_t*)&vram[0];
   int prio = lcd.bg[2].priority;
   int sizeX = 240;
   int sizeY = 160;

   int realX = lcd.bg[2].x_pos;
   int realY = lcd.bg[2].y_pos;

   if (lcd.bg[2].mosaic)
   {
      int mosaicY = ((lcd.mosaic & 0xF0) >> 4) + 1;
      int y = (lcd.vcount % mosaicY);
      realX -= y * lcd.bg[2].dmx;
      realY -= y * lcd.bg[2].dmy;
   }

   for (int x = 0; x < 240; ++x)
   {
      int xxx = (realX >> 8);
      int yyy = (realY >> 8);

      realX += lcd.bg[2].dx;
      realY += lcd.bg[2].dy;

      if (xxx < 0 || yyy < 0 || xxx >= sizeX || yyy >= sizeY)
      {
         lineBG[2][x] = 0x80000000;
         continue;
      }

      lineBG[2][x] = (READ16LE(&screenBase[yyy * sizeX + xxx]) | prio);
   }

   if (lcd.bg[2].mosaic)
   {
      int mosaicX = (lcd.mosaic & 0xF) + 1;
      if (mosaicX > 1)
      {
         int m = 1;
         for (int i = 0; i < 239; ++i)
         {
            lineBG[2][i + 1] = lineBG[2][i];
            if (++m == mosaicX)
            {
               m = 1;
               ++i;
            }
         }
      }
   }
}

void gfxDrawRotScreen256()
{
   if ((layerEnable & 0x0400) == 0)
      return;

   uint16_t* palette = (uint16_t*)paletteRAM;
   uint8_t* screenBase = (lcd.dispcnt & 0x0010) ? &vram[0xA000] : &vram[0x0000];
   int prio = lcd.bg[2].priority;
   int sizeX = 240;
   int sizeY = 160;

   int realX = lcd.bg[2].x_pos;
   int realY = lcd.bg[2].y_pos;

   if (lcd.bg[2].mosaic)
   {
      int mosaicY = ((lcd.mosaic & 0xF0) >> 4) + 1;
      int y = lcd.vcount - (lcd.vcount % mosaicY);
      realX = lcd.bg[2].x_ref + y * lcd.bg[2].dmx;
      realY = lcd.bg[2].y_ref + y * lcd.bg[2].dmy;
   }

   for (int x = 0; x < 240; ++x)
   {
      int xxx = (realX >> 8);
      int yyy = (realY >> 8);

      realX += lcd.bg[2].dx;
      realY += lcd.bg[2].dy;

      if (xxx < 0 || yyy < 0 || xxx >= sizeX || yyy >= sizeY)
      {
         lineBG[2][x] = 0x80000000;
         continue;
      }

      uint8_t color = screenBase[yyy * 240 + xxx];
      lineBG[2][x] = color ? (READ16LE(&palette[color]) | prio) : 0x80000000;
   }

   if (lcd.bg[2].mosaic)
   {
      int mosaicX = (lcd.mosaic & 0xF) + 1;
      if (mosaicX > 1)
      {
         int m = 1;
         for (int i = 0; i < 239; ++i)
         {
            lineBG[2][i + 1] = lineBG[2][i];
            if (++m == mosaicX)
            {
               m = 1;
               ++i;
            }
         }
      }
   }
}

void gfxDrawRotScreen16Bit160()
{
   if ((layerEnable & 0x0400) == 0)
      return;

   uint16_t* screenBase = (lcd.dispcnt & 0x0010) ? (uint16_t*)&vram[0xa000] : (uint16_t*)&vram[0];
   int prio = lcd.bg[2].priority;
   int sizeX = 160;
   int sizeY = 128;

   int realX = lcd.bg[2].x_pos;
   int realY = lcd.bg[2].y_pos;

   if (lcd.bg[2].mosaic)
   {
      int mosaicY = ((lcd.mosaic & 0xF0) >> 4) + 1;
      int y = lcd.vcount - (lcd.vcount % mosaicY);
      realX = lcd.bg[2].x_ref + y * lcd.bg[2].dmx;
      realY = lcd.bg[2].y_ref + y * lcd.bg[2].dmy;
   }

   for (int x = 0; x < 240; ++x)
   {
      int xxx = (realX >> 8);
      int yyy = (realY >> 8);

      realX += lcd.bg[2].dx;
      realY += lcd.bg[2].dy;

      if (xxx < 0 || yyy < 0 || xxx >= sizeX || yyy >= sizeY)
      {
         lineBG[2][x] = 0x80000000;
         continue;
      }

      lineBG[2][x] = (READ16LE(&screenBase[yyy * sizeX + xxx]) | prio);
   }

   if (lcd.bg[2].mosaic)
   {
      int mosaicX = (lcd.mosaic & 0xF) + 1;
      if (mosaicX > 1)
      {
         int m = 1;
         for (int i = 0; i < 239; ++i)
         {
            lineBG[2][i + 1] = lineBG[2][i];
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
   int lineOBJpix = (lcd.dispcnt & 0x20) ? 954 : 1226;
   int m = 0;
   gfxClearArray(lineOBJ);
   if (layerEnable & 0x1000)
   {
      uint16_t* sprites = (uint16_t*)oam;
      uint16_t* spritePalette = &((uint16_t*)paletteRAM)[256];
      int mosaicY = ((lcd.mosaic & 0xF000) >> 12) + 1;
      int mosaicX = ((lcd.mosaic & 0xF00) >> 8) + 1;
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
            if ((lcd.vcount >= sy) && (lcd.vcount < sy + sizeY) && (sx < 240))
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
            int t = lcd.vcount - sy;
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
                     if ((lcd.dispcnt & 7) > 2 && (c < 512))
                        continue;

                     if (a0 & 0x2000)
                     {
                        int inc = 32;
                        if (lcd.dispcnt & 0x40)
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
                        if (lcd.dispcnt & 0x40)
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
            int t = lcd.vcount - sy;
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
                     if ((lcd.dispcnt & 7) > 2 && (c < 512))
                        continue;

                     int inc = 32;
                     if (lcd.dispcnt & 0x40)
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
                     if ((lcd.dispcnt & 7) > 2 && (c < 512))
                        continue;

                     int inc = 32;
                     if (lcd.dispcnt & 0x40)
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
            int t = lcd.vcount - sy;
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
                     if ((lcd.dispcnt & 7) > 2 && (c < 512))
                        continue;
                     int inc = 32;
                     if (lcd.dispcnt & 0x40)
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
                     if ((lcd.dispcnt & 7) > 2 && (c < 512))
                        continue;

                     int inc = 32;
                     if (lcd.dispcnt & 0x40)
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
            int t = lcd.vcount - sy;
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
                     if ((lcd.dispcnt & 7) > 2 && (c < 512))
                        continue;

                     int inc = 32;
                     if (lcd.dispcnt & 0x40)
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
                     if ((lcd.dispcnt & 7) > 2 && (c < 512))
                        continue;

                     int inc = 32;
                     if (lcd.dispcnt & 0x40)
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

void LCDUpdateBGCNT(int layer, uint16_t value)
{
   lcd_background_t* bgcnt = &lcd.bg[layer];
   bgcnt->priority = ((value & 3) << 25) + 0x1000000;
   bgcnt->char_base = ((value >> 2) & 3) * 0x4000;
   bgcnt->mosaic = value & 0x40;
   bgcnt->palette256 = value & 0x80;
   bgcnt->screen_base = ((value >> 8) & 0x1F) * 0x800;
   bgcnt->overflow = value & 0x2000;
   bgcnt->screen_size = (value >> 14) & 3;
}

void LCDUpdateBGX_L(int layer, uint16_t value)
{
   lcd.bg[layer].x_ref &= 0xFFFF0000;
   lcd.bg[layer].x_ref |= value;
   lcd.bg[layer].x_pos = lcd.bg[layer].x_ref;
}

void LCDUpdateBGX_H(int layer, uint16_t value)
{
   lcd.bg[layer].x_ref &= 0x0000FFFF;
   lcd.bg[layer].x_ref |= (value << 16);
   if (value & 0x0800)
      lcd.bg[layer].x_ref |= 0xF8000000;
   lcd.bg[layer].x_pos = lcd.bg[layer].x_ref;
}

void LCDUpdateBGY_L(int layer, uint16_t value)
{
   lcd.bg[layer].y_ref &= 0xFFFF0000;
   lcd.bg[layer].y_ref |= value;
   lcd.bg[layer].y_pos = lcd.bg[layer].y_ref;
}
void LCDUpdateBGY_H(int layer, uint16_t value)
{
   lcd.bg[layer].y_ref &= 0x0000FFFF;
   lcd.bg[layer].y_ref |= (value << 16);
   if (value & 0x0800)
      lcd.bg[layer].y_ref |= 0xF8000000;
   lcd.bg[layer].y_pos = lcd.bg[layer].y_ref;
}

static bool isInWindow(int min, int max, int x)
{
   return x >= min && x < max;
}

void LCDUpdateWindow0(void)
{
   int x00 = lcd.winh[0] >> 8;  // Leftmost coordinate of window
   int x01 = lcd.winh[0] & 255; // Rightmost coordinate of window
   if (x01 > GBA_WIDTH || x00 > x01)
      x01 = GBA_WIDTH;
   for (int i = 0; i < GBA_WIDTH; ++i)
      gfxInWin0[i] = isInWindow(x00, x01, i);
}

void LCDUpdateWindow1(void)
{
   int x00 = lcd.winh[1] >> 8;  // Leftmost coordinate of window
   int x01 = lcd.winh[1] & 255; // Rightmost coordinate of window
   if (x01 > GBA_WIDTH || x00 > x01)
      x01 = GBA_WIDTH;
   for (int i = 0; i < 240; ++i)
      gfxInWin1[i] = isInWindow(x00, x01, i);
}

bool LCDUpdateInWindow(int which, uint16_t value, uint32_t vcount)
{
   if ((layerEnable & (0x2000 << which)) == 0)
      return false;

   uint8_t y00 = value >> 8;  // Top-most coordinate of window
   uint8_t y01 = value & 255; // Bottom-most coordinate of window
   if (y01 > GBA_HEIGHT || y00 > y01)
      y01 = GBA_HEIGHT;
   return isInWindow(y00, y01, vcount);
}

void LCDResetBGRegisters()
{
   for (int i = 0; i < 4; ++i)
   {
      lcd_background_t* bg = &lcd.bg[i];
      bg->index = i;
      bg->priority = 0;
      bg->char_base = 0;
      bg->mosaic = 0;
      bg->palette256 = 0;
      bg->screen_base = 0;
      bg->overflow = 0;
      bg->screen_size = 0;
      bg->hofs = 0;
      bg->vofs = 0;
      bg->dx = 256;
      bg->dmx = 0;
      bg->dy = 0;
      bg->dmy = 256;
      bg->x_ref = 0;
      bg->y_ref = 0;
      bg->x_pos = 0;
      bg->y_pos = 0;
   }
}

void LCDUpdateBGRegisters()
{
   LCDUpdateBGCNT(0, BG0CNT);
   LCDUpdateBGCNT(1, BG1CNT);
   LCDUpdateBGCNT(2, BG2CNT);
   LCDUpdateBGCNT(3, BG3CNT);
}

#define RENDERLINE(MODE)                                              \
   if (renderer_type == 0)      mode##MODE##RenderLine(dest);         \
   else if (renderer_type == 1) mode##MODE##RenderLineNoWindow(dest); \
   else if (renderer_type == 2) mode##MODE##RenderLineAll(dest);      \

void gfxDrawScanline(pixFormat* dest, int renderer_mode, int renderer_type)
{
   if (lcd.dispcnt & 0x80) // Force Blank
   {
      // draw white
      forceBlankLine(dest);
      return;
   }
   if (lcd.vcount == 0)
   {
      lcd.bg[2].x_pos = lcd.bg[2].x_ref;
      lcd.bg[2].y_pos = lcd.bg[2].y_ref;
      lcd.bg[3].x_pos = lcd.bg[3].x_ref;
      lcd.bg[3].y_pos = lcd.bg[3].y_ref;
   }
   // Draw scanline pix to screen buffer
   gfxDrawSprites();
   if (renderer_type == 2)
      gfxDrawOBJWin();
   switch (renderer_mode)
   {
      case 0:
         gfxDrawTextScreen(0);
         gfxDrawTextScreen(1);
         gfxDrawTextScreen(2);
         gfxDrawTextScreen(3);
         RENDERLINE(0);
         break;
      case 1:
         gfxDrawTextScreen(0);
         gfxDrawTextScreen(1);
         gfxDrawRotScreen(2);
         RENDERLINE(1);
         break;
      case 2:
         gfxDrawRotScreen(2);
         gfxDrawRotScreen(3);
         RENDERLINE(2);
         break;
      case 3:
         gfxDrawRotScreen16Bit();
         RENDERLINE(3);
         break;
      case 4:
         gfxDrawRotScreen256();
         RENDERLINE(4);
         break;
      case 5:
         gfxDrawRotScreen16Bit160();
         RENDERLINE(5);
         break;
   }
   if ((lcd.dispcnt & 7) != 0)
   {
      if (layerEnable & 0x0400)
      {
         lcd.bg[2].x_pos += lcd.bg[2].dmx;
         lcd.bg[2].y_pos += lcd.bg[2].dmy;
      }
      if (layerEnable & 0x0800)
      {
         lcd.bg[3].x_pos += lcd.bg[3].dmx;
         lcd.bg[3].y_pos += lcd.bg[3].dmy;
      }
   }
}
