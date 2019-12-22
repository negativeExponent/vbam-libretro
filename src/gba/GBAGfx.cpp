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

gba_lcd_t lcd;

static INLINE void gfxClearArray(uint32_t* array)
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

#define BG_MOSAIC_LOOP(m_layer)                       \
   if (lcd.bg_mosaic_hsize > 1)                       \
   {                                                  \
      int m = 1;                                      \
      for (int i = 0; i < 239; ++i)                   \
      {                                               \
         lineBG[m_layer][i + 1] = lineBG[m_layer][i]; \
         if (++m == lcd.bg_mosaic_hsize)              \
         {                                            \
            m = 1;                                    \
            ++i;                                      \
         }                                            \
      }                                               \
   }

#ifdef TILED_RENDERING
struct TileLine
{
   uint32_t pixels[8];
};

typedef const TileLine (*TileReader)(const uint16_t*, const int, const uint8_t*, uint16_t*, const uint32_t);

static INLINE void gfxDrawPixel(uint32_t* dest, const uint8_t color, const uint16_t* palette, const uint32_t prio)
{
   *dest = color ? (READ16LE(&palette[color]) | prio) : 0x80000000;
}

#define BIT(x) (1 << (x))
#define TILENUM(x) ((x) & 0x03FF)
#define TILEPALETTE(x) (((x) >> 12) & 0x000F)
#define HFLIP BIT(10)
#define VFLIP BIT(11)

INLINE const TileLine gfxReadTile(const uint16_t* screenSource, const int yyy, const uint8_t* charBase, uint16_t* palette, const uint32_t prio)
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

INLINE const TileLine gfxReadTilePal(const uint16_t* screenSource, const int yyy, const uint8_t* charBase, uint16_t* palette, const uint32_t prio)
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

static INLINE void gfxDrawTile(const TileLine& tileLine, uint32_t* line)
{
   memcpy(line, tileLine.pixels, sizeof(tileLine.pixels));
}

static INLINE void gfxDrawTileClipped(const TileLine& tileLine, uint32_t* line, const int start, int w)
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

   int maskX = lcd.bg[layer].sizeX - 1;
   int maskY = lcd.bg[layer].sizeY - 1;

   int xxx = lcd.bg[layer].hofs & maskX;
   int yyy = (lcd.bg[layer].vofs + lcd.vcount) & maskY;

   if (lcd.bg[layer].mosaic)
   {
      if ((lcd.vcount % lcd.bg_mosaic_vsize) != 0)
      {
         int y = lcd.vcount - (lcd.vcount % lcd.bg_mosaic_vsize);
         yyy = (lcd.bg[layer].vofs + y) & maskY;
      }
   }

   if (yyy > 255 && lcd.bg[layer].sizeY > 256)
   {
      yyy &= 255;
      screenBase += 0x400;
      if (lcd.bg[layer].sizeX > 256)
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

      if (xxx == 256 && lcd.bg[layer].sizeX > 256)
      {
         screenSource = screenBase + 0x400 + yshift;
      }
      else if (xxx >= lcd.bg[layer].sizeX)
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

      if (xxx == 256 && lcd.bg[layer].sizeX > 256)
      {
         screenSource = screenBase + 0x400 + yshift;
         continue;
      }
      if (xxx >= lcd.bg[layer].sizeX)
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

   if (lcd.bg[layer].mosaic)
   {
      BG_MOSAIC_LOOP(layer);
   }
}

void gfxDrawTextScreen(int layer)
{
   if ((layerEnable & (0x0100 << layer)) == 0)
      return;
   if (lcd.bg[layer].color256) // 1 pal / 256 col
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

   int maskX = lcd.bg[layer].sizeX - 1;
   int maskY = lcd.bg[layer].sizeY - 1;

   int xxx = lcd.bg[layer].hofs & maskX;
   int yyy = (lcd.bg[layer].vofs + lcd.vcount) & maskY;

   if (lcd.bg[layer].mosaic)
   {
      if ((lcd.vcount % lcd.bg_mosaic_vsize) != 0)
      {
         int y = lcd.vcount - (lcd.vcount % lcd.bg_mosaic_vsize);
         yyy = (lcd.bg[layer].vofs + y) & maskY;
      }
   }

   if (yyy > 255 && lcd.bg[layer].sizeY > 256)
   {
      yyy &= 255;
      screenBase += 0x400;
      if (lcd.bg[layer].sizeX > 256)
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
            if (lcd.bg[layer].sizeX > 256)
               screenSource = screenBase + 0x400 + yshift;
            else
            {
               screenSource = screenBase + yshift;
               xxx = 0;
            }
         }
         else if (xxx >= lcd.bg[layer].sizeX)
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
            if (lcd.bg[layer].sizeX > 256)
               screenSource = screenBase + 0x400 + yshift;
            else
            {
               screenSource = screenBase + yshift;
               xxx = 0;
            }
         }
         else if (xxx >= lcd.bg[layer].sizeX)
         {
            xxx = 0;
            screenSource = screenBase + yshift;
         }
      }
   }
   if (lcd.bg[layer].mosaic)
   {
      BG_MOSAIC_LOOP(layer);
   }
}
#endif // !__TILED_RENDERING

template <typename T>
static INLINE T MAX(T a, T b)
{
   return (a > b) ? a : b;
}

template <typename T>
static INLINE T MIN(T a, T b)
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

   int maskX = lcd.bg[layer].sizeX - 1;
   int maskY = lcd.bg[layer].sizeY - 1;

   int yshift = lcd.bg[layer].screen_size + 4;

   int realX = lcd.bg[layer].x_pos;
   int realY = lcd.bg[layer].y_pos;

   if (lcd.bg[layer].mosaic)
   {
      int y = (lcd.vcount % lcd.bg_mosaic_vsize);
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

         if (yyy < 0 || yyy >= lcd.bg[layer].sizeY)
            goto skipLine;

         int x0 = MAX<int32_t>(0, (int32_t)(+(-realX + lcd.bg[layer].dx - 1)) / lcd.bg[layer].dx);
         int x1 = MIN<int32_t>(240, (int32_t)((lcd.bg[layer].sizeX << 8) + (-realX + lcd.bg[layer].dx - 1)) / lcd.bg[layer].dx);

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

            if (xxx < 0 || yyy < 0 || xxx >= lcd.bg[layer].sizeX || yyy >= lcd.bg[layer].sizeY)
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
      BG_MOSAIC_LOOP(layer);
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
      int y = (lcd.vcount % lcd.bg_mosaic_vsize);
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
      BG_MOSAIC_LOOP(2);
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
      int y = lcd.vcount - (lcd.vcount % lcd.bg_mosaic_vsize);
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
      BG_MOSAIC_LOOP(2);
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
      int y = lcd.vcount - (lcd.vcount % lcd.bg_mosaic_vsize);
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
      BG_MOSAIC_LOOP(2);
   }
}

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

struct obj_attribute_t
{
   // obj attribute 0
   int  startY;
   int  mode;
   int  shape;
   bool affineOn;
   bool doubleSizeFlag;
   bool mosaicOn;
   bool color256;

   // obj attribute 1
   int  startX;
   int  size;
   bool hFlip;
   bool vFlip;

   // obj attribute 2
   int tile;
   int affineGroup;
   int priority;
   int palette;

   // misc
   uint32_t prio;
   bool objDisable;
   int sizeX;
   int sizeY;

   // affine
   int dx;
   int dmx;
   int dy;
   int dmy;
   int fieldX;
   int fieldY;

   int lineOBJpixleft;
};

bool oam_updated;
bool oam_obj_updated[128];
static obj_attribute_t obj[128];

static INLINE void update_oam()
{
   uint16_t* sprites = (uint16_t*)oam;
   uint16_t value = 0;
   oam_updated = false;
   for (int num = 0; num < 128; ++num)
   {
      if (oam_obj_updated[num])
      {
         oam_obj_updated[num] = false;

         // Read obj attribute 0
         value = READ16LE(sprites++);

         if ((value & 0x0C00) == 0x0C00)
            value &= 0xF3FF;

         obj[num].startY         = (value & 255);
         obj[num].affineOn       = (value & 0x100) ? true : false;
         obj[num].doubleSizeFlag = (value & 0x200) ? true : false;
         obj[num].mode           = (value & 0x0C00) >> 10;
         obj[num].mosaicOn       = (value & 0x1000) ? true : false;
         obj[num].color256       = (value & 0x2000) ? true : false;
         obj[num].shape          = (value & 0xC000) >> 14;

         obj[num].objDisable = false;
         if ((obj[num].affineOn == false) && obj[num].doubleSizeFlag)
            obj[num].objDisable = true;

         // Read obj attribute 1
         value = READ16LE(sprites++);
         obj[num].startX        = (value & 0x1FF);
         obj[num].hFlip         = (value & 0x1000) ? true : false;
         obj[num].vFlip         = (value & 0x2000) ? true : false;
         obj[num].size          = (value & 0xC000) >> 14;

         obj[num].affineGroup = 0;
         if (obj[num].affineOn)
            obj[num].affineGroup = (value & 0x3E00) >> 9;

         // Read obj attribute 2
         value = READ16LE(sprites++);
         obj[num].tile          = (value & 0x3FF);
         obj[num].priority      = (value & 0xC00) >> 10;
         obj[num].palette       = ((value & 0xF000) >> 12) << 4;

         ++sprites;

         // pre-calculate internal stuff
         obj[num].sizeX = objSizeMapX[obj[num].size][obj[num].shape];
         obj[num].sizeY = objSizeMapY[obj[num].size][obj[num].shape];
         obj[num].prio  = (obj[num].priority << 25) | (obj[num].mode << 16);
      }

      // skip to next obj group
      else
         sprites += 4;
   }

   // pre-calculate affine parameters if enabled
   uint16_t* OAM = (uint16_t*)oam;
   for (int num = 0; num < 128; ++num)
   {
      if (obj[num].affineOn == false)
         continue;
      int rot = obj[num].affineGroup;
      obj[num].dx = READ16LE(&OAM[3 + (rot << 4)]);
      if (obj[num].dx & 0x8000)
         obj[num].dx |= 0xFFFF8000;
      obj[num].dmx = READ16LE(&OAM[7 + (rot << 4)]);
      if (obj[num].dmx & 0x8000)
         obj[num].dmx |= 0xFFFF8000;
      obj[num].dy = READ16LE(&OAM[11 + (rot << 4)]);
      if (obj[num].dy & 0x8000)
         obj[num].dy |= 0xFFFF8000;
      obj[num].dmy = READ16LE(&OAM[15 + (rot << 4)]);
      if (obj[num].dmy & 0x8000)
         obj[num].dmy |= 0xFFFF8000;

      obj[num].fieldX = obj[num].sizeX;
      obj[num].fieldY = obj[num].sizeY;
      if (obj[num].doubleSizeFlag)
      {
         obj[num].fieldX <<= 1;
         obj[num].fieldY <<= 1;
      }
   }
}

#define OBJ_MOSAIC_LOOP(m_sx, m_num)                                         \
   if (lcd.obj_mosaic_hsize > 1)                                             \
   {                                                                         \
      if (m)                                                                 \
         lineOBJ[m_sx] = (lineOBJ[m_sx - 1] & 0xF9FFFFFF) | obj[m_num].prio; \
      if (++m == lcd.obj_mosaic_hsize)                                       \
         m = 0;                                                              \
   }

static void draw_obj_affine_transformation(int num, int16_t& lineOBJpix, int& m)
{
   int sy = obj[num].startY;
   int sx = obj[num].startX;

#ifdef SPRITE_DEBUG
      int maskX = obj[num].sizeX - 1;
      int maskY = obj[num].sizeY - 1;
#endif

   if ((sy + obj[num].fieldY) > 256)
      sy -= 256;
   int t = lcd.vcount - sy;
   if ((t >= 0) && (t < obj[num].fieldY))
   {
      int startpix = 0;
      if ((sx + obj[num].fieldX) > 512)
      {
         startpix = 512 - sx;
      }
      if ((sx < 240) || startpix)
      {
         lineOBJpix -= 8;

         if (obj[num].mosaicOn)
         {
            t -= (t % lcd.obj_mosaic_vsize);
         }

         int realX = (obj[num].sizeX << 7) - (obj[num].fieldX >> 1) * obj[num].dx - (obj[num].fieldY >> 1) * obj[num].dmx + t * obj[num].dmx;
         int realY = (obj[num].sizeY << 7) - (obj[num].fieldX >> 1) * obj[num].dy - (obj[num].fieldY >> 1) * obj[num].dmy + t * obj[num].dmy;

         int c = (obj[num].tile);
         if ((lcd.dispcnt & 7) > 2 && (c < 512))
            return;

         uint16_t* spritePalette = &((uint16_t*)paletteRAM)[256];

         // color 256 / palette 1
         if (obj[num].color256)
         {
            int inc = 32;
            if (lcd.dispcnt & 0x40)
               inc = obj[num].sizeX >> 2;
            else
               c &= 0x3FE;
            for (int xx = 0; xx < obj[num].fieldX; ++xx)
            {
               if (xx >= startpix)
                  lineOBJpix -= 2;
               if (lineOBJpix < 0)
                  return;
               int xxx = realX >> 8;
               int yyy = realY >> 8;
               if (xxx < 0 || xxx >= obj[num].sizeX || yyy < 0 || yyy >= obj[num].sizeY || sx >= 240)
               {
               }
               else
               {
                  uint32_t color = vram[0x10000 + ((((c + (yyy >> 3) * inc) << 5) + ((yyy & 7) << 3) + ((xxx >> 3) << 6) + (xxx & 7)) & 0x7FFF)];
                  if ((color == 0) && (((obj[num].prio >> 25) & 3) < ((lineOBJ[sx] >> 25) & 3)))
                  {
                     lineOBJ[sx] = (lineOBJ[sx] & 0xF9FFFFFF) | obj[num].prio;
                  }
                  else if (color && (obj[num].prio < (lineOBJ[sx] & 0xFF000000)))
                  {
                     lineOBJ[sx] = READ16LE(&spritePalette[color]) | obj[num].prio;
                  }
               }

               if (obj[num].mosaicOn)
               {
                  OBJ_MOSAIC_LOOP(sx, num);
               }

#ifdef SPRITE_DEBUG
                  if (t == 0 || t == maskY || xx == 0 || xx == maskX)
                     lineOBJ[sx] = 0x001F;
#endif
               sx = (sx + 1) & 511;
               realX += obj[num].dx;
               realY += obj[num].dy;
            }
         }

         // color 16 / palette 16
         else
         {
            int inc = 32;
            if (lcd.dispcnt & 0x40)
               inc = obj[num].sizeX >> 3;
            for (int xx = 0; xx < obj[num].fieldX; ++xx)
            {
               if (xx >= startpix)
                  lineOBJpix -= 2;
               if (lineOBJpix < 0)
                  return;
               int xxx = realX >> 8;
               int yyy = realY >> 8;
               if (xxx < 0 || xxx >= obj[num].sizeX || yyy < 0 || yyy >= obj[num].sizeY || sx >= 240)
               {
               }
               else
               {
                  uint32_t color = vram[0x10000 + ((((c + (yyy >> 3) * inc) << 5) + ((yyy & 7) << 2) + ((xxx >> 3) << 5) + ((xxx & 7) >> 1)) & 0x7FFF)];
                  if (xxx & 1)
                     color >>= 4;
                  else
                     color &= 0x0F;

                  if ((color == 0) && (((obj[num].prio >> 25) & 3) < ((lineOBJ[sx] >> 25) & 3)))
                  {
                     lineOBJ[sx] = (lineOBJ[sx] & 0xF9FFFFFF) | obj[num].prio;
                  }
                  else if (color && (obj[num].prio < (lineOBJ[sx] & 0xFF000000)))
                  {
                     lineOBJ[sx] = READ16LE(&spritePalette[obj[num].palette + color]) | obj[num].prio;
                  }
               }

               if (obj[num].mosaicOn)
               {
                  OBJ_MOSAIC_LOOP(sx, num);
               }

#ifdef SPRITE_DEBUG
               if (t == 0 || t == maskY || xx == 0 || xx == maskX)
                  lineOBJ[sx] = 0x001F;
#endif
               sx = (sx + 1) & 511;
               realX += obj[num].dx;
               realY += obj[num].dy;
            }
         }
      }
   }
}

static void draw_obj_normal(int num, int16_t& lineOBJpix, int& m)
{
   int sy = obj[num].startY;
   int sx = obj[num].startX;

#ifdef SPRITE_DEBUG
      int maskX = obj[num].sizeX - 1;
      int maskY = obj[num].sizeY - 1;
#endif

   if (sy + obj[num].sizeY > 256)
      sy -= 256;
   int t = lcd.vcount - sy;
   if ((t >= 0) && (t < obj[num].sizeY))
   {
      int startpix = 0;
      if ((sx + obj[num].sizeX) > 512)
      {
         startpix = 512 - sx;
      }
      if ((sx < 240) || startpix)
      {
         lineOBJpix += 2;

         int c = (obj[num].tile);
         if ((lcd.dispcnt & 7) > 2 && (c < 512))
            return;

         uint16_t* spritePalette = &((uint16_t*)paletteRAM)[256];

         // color 256 / palette 1
         if (obj[num].color256)
         {
            if (obj[num].vFlip)
               t = obj[num].sizeY - t - 1;

            int inc = 32;
            if (lcd.dispcnt & 0x40)
            {
               inc = obj[num].sizeX >> 2;
            }
            else
            {
               c &= 0x3FE;
            }
            int xxx = 0;
            if (obj[num].hFlip)
               xxx = obj[num].sizeX - 1;

            if (obj[num].mosaicOn)
            {
               t -= (t % lcd.obj_mosaic_vsize);
            }

            int address = 0x10000 + ((((c + (t >> 3) * inc) << 5) + ((t & 7) << 3) + ((xxx >> 3) << 6) + (xxx & 7)) & 0x7FFF);

            if (obj[num].hFlip)
               xxx = 7;

            for (int xx = 0; xx < obj[num].sizeX; ++xx)
            {
               if (xx >= startpix)
                  --lineOBJpix;
               if (lineOBJpix < 0)
                  return;
               if (sx < 240)
               {
                  uint8_t color = vram[address];
                  if ((color == 0) && (((obj[num].prio >> 25) & 3) < ((lineOBJ[sx] >> 25) & 3)))
                  {
                     lineOBJ[sx] = (lineOBJ[sx] & 0xF9FFFFFF) | obj[num].prio;
                  }
                  else if (color && (obj[num].prio < (lineOBJ[sx] & 0xFF000000)))
                  {
                     lineOBJ[sx] = READ16LE(&spritePalette[color]) | obj[num].prio;
                  }
               }

               if (obj[num].mosaicOn)
               {
                  OBJ_MOSAIC_LOOP(sx, num);
               }

#ifdef SPRITE_DEBUG
                  if (t == 0 || t == maskY || xx == 0 || xx == maskX)
                     lineOBJ[sx] = 0x001F;
#endif

               sx = (sx + 1) & 511;
               if (obj[num].hFlip)
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

         // color 16 / palette 16
         else
         {
            if (obj[num].vFlip)
            {
               t = obj[num].sizeY - t - 1;
            }

            int inc = 32;
            if (lcd.dispcnt & 0x40)
            {
               inc = obj[num].sizeX >> 3;
            }
            int xxx = 0;
            if (obj[num].hFlip)
            {
               xxx = obj[num].sizeX - 1;
            }

            if (obj[num].mosaicOn)
            {
               t -= (t % lcd.obj_mosaic_vsize);
            }

            int address = 0x10000 + ((((c + (t >> 3) * inc) << 5) + ((t & 7) << 2) + ((xxx >> 3) << 5) + ((xxx & 7) >> 1)) & 0x7FFF);

            if (obj[num].hFlip)
            {
               xxx = 7;
               for (int xx = obj[num].sizeX - 1; xx >= 0; --xx)
               {
                  if (xx >= startpix)
                     --lineOBJpix;
                  if (lineOBJpix < 0)
                     return;
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

                     if ((color == 0) && (((obj[num].prio >> 25) & 3) < ((lineOBJ[sx] >> 25) & 3)))
                     {
                        lineOBJ[sx] = (lineOBJ[sx] & 0xF9FFFFFF) | obj[num].prio;
                     }
                     else if (color && (obj[num].prio < (lineOBJ[sx] & 0xFF000000)))
                     {
                        lineOBJ[sx] = READ16LE(&spritePalette[obj[num].palette + color]) | obj[num].prio;
                     }
                  }

                  if (obj[num].mosaicOn)
                  {
                     OBJ_MOSAIC_LOOP(sx, num);
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
               for (int xx = 0; xx < obj[num].sizeX; ++xx)
               {
                  if (xx >= startpix)
                  {
                     --lineOBJpix;
                  }
                  if (lineOBJpix < 0)
                  {
                     return;
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

                     if ((color == 0) && (((obj[num].prio >> 25) & 3) < ((lineOBJ[sx] >> 25) & 3)))
                     {
                        lineOBJ[sx] = (lineOBJ[sx] & 0xF9FFFFFF) | obj[num].prio;
                     }
                     else if (color && (obj[num].prio < (lineOBJ[sx] & 0xFF000000)))
                     {
                        lineOBJ[sx] = READ16LE(&spritePalette[obj[num].palette + color]) | obj[num].prio;
                     }
                  }

                  if (obj[num].mosaicOn)
                  {
                     OBJ_MOSAIC_LOOP(sx, num);
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

// computes ticks used by OBJ-WIN if OBJWIN is enabled
static INLINE void update_objwin_lineOBJpix(int num, int16_t& lineOBJpix)
{
   int sizeX = obj[num].sizeX;
   int sizeY = obj[num].sizeY;

   int sy = obj[num].startY;
   int sx = obj[num].startX;

   if (obj[num].affineOn && obj[num].doubleSizeFlag)
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
      if (obj[num].affineOn)
         lineOBJpix -= 8 + 2 * sizeX;
      else
         lineOBJpix -= sizeX - 2;
   }
}

#define MODE_OBJWIN 2

void gfxDrawSprites()
{
   gfxClearArray(lineOBJ);
   if ((layerEnable & 0x1000) == 0)
      return;
   // lineOBJpix is used to keep track of the drawn OBJs
   // and to stop drawing them if the 'maximum number of OBJ per line'
   // has been reached.
   int16_t lineOBJpix = (lcd.dispcnt & 0x20) ? 954 : 1210;//1226;
   int m = 0;
   for (int num = 0; num < 128; ++num)
   {
      obj[num].lineOBJpixleft = lineOBJpix;
      lineOBJpix -= 2;
      if (lineOBJpix <= 0)
      {
         // make sure OBJwin, if used would stop as well where is should
         obj[num].lineOBJpixleft = 0;
         return;
      }

      // computes ticks used by OBJ-WIN if OBJWIN is enabled
      if ((obj[num].mode == MODE_OBJWIN) && (layerEnable & 0x8000))
      {
         update_objwin_lineOBJpix(num, lineOBJpix);
         continue;
      }

      // else ignores OBJ-WIN if OBJWIN is disabled, and ignored disabled OBJ
      else if ((obj[num].mode == MODE_OBJWIN) || obj[num].objDisable)
         continue;

      if (obj[num].affineOn) draw_obj_affine_transformation(num, lineOBJpix, m);
      else draw_obj_normal(num, lineOBJpix, m);
   }
}

void gfxDrawOBJWin()
{
   gfxClearArray(lineOBJWin);
   if ((layerEnable & 0x9000) == 0x9000)
   {
      for (int num = 0; num < 128; ++num)
      {
         int lineOBJpix = obj[num].lineOBJpixleft;
         if (lineOBJpix <= 0)
            return;

         // ignores non OBJ-WIN and disabled OBJ-WIN
         if ((obj[num].mode != MODE_OBJWIN) || obj[num].objDisable)
            continue;

         int sx = (obj[num].startX);
         int sy = obj[num].startY;

         if (obj[num].affineOn)
         {
            if ((sy + obj[num].fieldY) > 256)
               sy -= 256;
            int t = lcd.vcount - sy;
            if ((t >= 0) && (t < obj[num].fieldY))
            {
               int startpix = 0;
               if ((sx + obj[num].fieldX) > 512)
               {
                  startpix = 512 - sx;
               }
               if ((sx < 240) || startpix)
               {
                  lineOBJpix -= 8;

                  int realX = ((obj[num].sizeX) << 7) - (obj[num].fieldX >> 1) * obj[num].dx - (obj[num].fieldY >> 1) * obj[num].dmx + t * obj[num].dmx;
                  int realY = ((obj[num].sizeY) << 7) - (obj[num].fieldX >> 1) * obj[num].dy - (obj[num].fieldY >> 1) * obj[num].dmy + t * obj[num].dmy;

                  int c = (obj[num].tile);
                  if ((lcd.dispcnt & 7) > 2 && (c < 512))
                     continue;

                  if (obj[num].color256)
                  {
                     int inc = 32;
                     if (lcd.dispcnt & 0x40)
                        inc = obj[num].sizeX >> 2;
                     else
                        c &= 0x3FE;
                     for (int xx = 0; xx < obj[num].fieldX; ++xx)
                     {
                        if (xx >= startpix)
                           lineOBJpix -= 2;
                        if (lineOBJpix < 0)
                           continue;
                        int xxx = realX >> 8;
                        int yyy = realY >> 8;

                        if (xxx < 0 || xxx >= obj[num].sizeX || yyy < 0 || yyy >= obj[num].sizeY || sx >= 240)
                        {
                        }
                        else
                        {
                           uint32_t color = vram[0x10000 + ((((c + (yyy >> 3) * inc) << 5) + ((yyy & 7) << 3) + ((xxx >> 3) << 6) + (xxx & 7)) & 0x7FFF)];
                           if (color)
                           {
                              lineOBJWin[sx] = 1;
                           }
                        }
                        sx = (sx + 1) & 511;
                        realX += obj[num].dx;
                        realY += obj[num].dy;
                     }
                  }
                  else // color 16 / palette 16
                  {
                     int inc = 32;
                     if (lcd.dispcnt & 0x40)
                        inc = obj[num].sizeX >> 3;
                     for (int xx = 0; xx < obj[num].fieldX; ++xx)
                     {
                        if (xx >= startpix)
                           lineOBJpix -= 2;
                        if (lineOBJpix < 0)
                           continue;
                        int xxx = realX >> 8;
                        int yyy = realY >> 8;

                        if (xxx < 0 || xxx >= obj[num].sizeX || yyy < 0 || yyy >= obj[num].sizeY || sx >= 240)
                        {
                        }
                        else
                        {
                           uint32_t color = vram[0x10000 + ((((c + (yyy >> 3) * inc) << 5) + ((yyy & 7) << 2) + ((xxx >> 3) << 5) + ((xxx & 7) >> 1)) & 0x7fff)];
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
                        realX += obj[num].dx;
                        realY += obj[num].dy;
                     }
                  }
               }
            }
         }
         else
         {
            if ((sy + obj[num].sizeY) > 256)
               sy -= 256;
            int t = lcd.vcount - sy;
            if ((t >= 0) && (t < obj[num].sizeY))
            {
               int startpix = 0;
               if ((sx + obj[num].sizeX) > 512)
               {
                  startpix = 512 - sx;
               }
               if ((sx < 240) || startpix)
               {
                  lineOBJpix += 2;

                  int c = (obj[num].tile);
                  if ((lcd.dispcnt & 7) > 2 && (c < 512))
                     continue;

                  if (obj[num].color256) // color 256 / palette 1
                  {
                     if (obj[num].vFlip)
                        t = obj[num].sizeY - t - 1;

                     int inc = 32;
                     if (lcd.dispcnt & 0x40)
                     {
                        inc = obj[num].sizeX >> 2;
                     }
                     else
                     {
                        c &= 0x3FE;
                     }
                     int xxx = 0;
                     if (obj[num].hFlip)
                        xxx = obj[num].sizeX - 1;
                     int address = 0x10000 + ((((c + (t >> 3) * inc) << 5) + ((t & 7) << 3) + ((xxx >> 3) << 6) + (xxx & 7)) & 0x7fff);
                     if (obj[num].hFlip)
                        xxx = 7;
                     for (int xx = 0; xx < obj[num].sizeX; ++xx)
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
                        if (obj[num].hFlip)
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
                  else // color 16 / palette 16
                  {
                     if (obj[num].vFlip)
                        t = obj[num].sizeY - t - 1;

                     int inc = 32;
                     if (lcd.dispcnt & 0x40)
                     {
                        inc = obj[num].sizeX >> 3;
                     }
                     int xxx = 0;
                     if (obj[num].hFlip)
                        xxx = obj[num].sizeX - 1;
                     int address = 0x10000 + ((((c + (t >> 3) * inc) << 5) + ((t & 7) << 2) + ((xxx >> 3) << 5) + ((xxx & 7) >> 1)) & 0x7fff);

                     if (obj[num].hFlip)
                     {
                        xxx = 7;
                        for (int xx = obj[num].sizeX - 1; xx >= 0; --xx)
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
                                 lineOBJWin[sx] = 1;
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
                        for (int xx = 0; xx < obj[num].sizeX; ++xx)
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
                                 lineOBJWin[sx] = 1;
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
   bgcnt->color256 = value & 0x80;
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

void LCDUpdateMOSAIC(uint16_t value)
{
   lcd.bg_mosaic_hsize  = ((value & 0x000F) >> 0) + 1;
   lcd.bg_mosaic_vsize  = ((value & 0x00F0) >> 4) + 1;
   lcd.obj_mosaic_hsize = ((value & 0x0F00) >> 8) + 1;
   lcd.obj_mosaic_vsize = ((value & 0xF000) >> 12) + 1;
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
      bg->color256 = 0;
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
   //update_oam();
   oam_updated = true;
   for (int x = 0; x < 128; ++x) oam_obj_updated[x] = true;
}

void LCDUpdateBGRegisters()
{
   LCDUpdateBGCNT(0, READ16LE((uint16_t*)&ioMem[REG_BG0CNT]));
   LCDUpdateBGCNT(1, READ16LE((uint16_t*)&ioMem[REG_BG1CNT]));
   LCDUpdateBGCNT(2, READ16LE((uint16_t*)&ioMem[REG_BG2CNT]));
   LCDUpdateBGCNT(3, READ16LE((uint16_t*)&ioMem[REG_BG3CNT]));
   LCDUpdateMOSAIC(READ16LE((uint16_t*)&ioMem[REG_MOSAIC]));
}

#define RENDERER_TYPE_SIMPLE   0
#define RENDERER_TYPE_NOWINDOW 1
#define RENDERER_TYPE_ALL      2

#define RENDERLINE(MODE)                                                                   \
   if (renderer_type == RENDERER_TYPE_SIMPLE)        mode## MODE ##RenderLine(dest);         \
   else if (renderer_type == RENDERER_TYPE_NOWINDOW) mode## MODE ##RenderLineNoWindow(dest); \
   else if (renderer_type == RENDERER_TYPE_ALL)      mode## MODE ##RenderLineAll(dest);      \

void gfxDrawScanline(pixFormat* dest, int renderer_mode, int renderer_type)
{
   /*log("drawScanline: Mode = %d Type = %d scanline = %3d\n",
      renderer_mode, renderer_type, lcd.vcount);*/

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

   // pre-calculate some stuff we need to use
   if (oam_updated)
      update_oam();
   switch (renderer_mode)
   {
      case 0:
         lcd.bg[0].sizeX = textModeSizeMapX[lcd.bg[0].screen_size];
         lcd.bg[0].sizeY = textModeSizeMapY[lcd.bg[0].screen_size];
         lcd.bg[1].sizeX = textModeSizeMapX[lcd.bg[1].screen_size];
         lcd.bg[1].sizeY = textModeSizeMapY[lcd.bg[1].screen_size];
         lcd.bg[2].sizeX = textModeSizeMapX[lcd.bg[2].screen_size];
         lcd.bg[2].sizeY = textModeSizeMapY[lcd.bg[2].screen_size];
         lcd.bg[3].sizeX = textModeSizeMapX[lcd.bg[3].screen_size];
         lcd.bg[3].sizeY = textModeSizeMapY[lcd.bg[3].screen_size];
         break;
      case 1:
         lcd.bg[0].sizeX = textModeSizeMapX[lcd.bg[0].screen_size];
         lcd.bg[0].sizeY = textModeSizeMapY[lcd.bg[0].screen_size];
         lcd.bg[1].sizeX = textModeSizeMapX[lcd.bg[1].screen_size];
         lcd.bg[1].sizeY = textModeSizeMapY[lcd.bg[1].screen_size];
         lcd.bg[2].sizeX = rotationModeSizeMap[lcd.bg[2].screen_size];
         lcd.bg[2].sizeY = rotationModeSizeMap[lcd.bg[2].screen_size];
         break;
      case 2:
         lcd.bg[2].sizeX = rotationModeSizeMap[lcd.bg[2].screen_size];
         lcd.bg[2].sizeY = rotationModeSizeMap[lcd.bg[2].screen_size];
         lcd.bg[3].sizeX = rotationModeSizeMap[lcd.bg[3].screen_size];
         lcd.bg[3].sizeY = rotationModeSizeMap[lcd.bg[3].screen_size];
         break;
      default: break;
   }

   gfxDrawSprites();
   if (renderer_type == RENDERER_TYPE_ALL)
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
      default: break;
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
