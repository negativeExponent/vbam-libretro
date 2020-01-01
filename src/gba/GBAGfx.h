#ifndef GFX_H
#define GFX_H

#include "GBA.h"
#include "Globals.h"
#include "../common/Port.h"

struct lcd_background_t
{
    int index;

    // BGCNT
    int priority;
    int char_base;
    int mosaic;
    int color256;
    int screen_base;
    int overflow;
    int screen_size;
    // screen dimensions
    int sizeX;
    int sizeY;

    // Horizontal/Vertical Offsets    
    uint16_t hofs;
    uint16_t vofs;
    
    // BG Affine
    int16_t dx;
    int16_t dmx;
    int16_t dy;
    int16_t dmy;
    int32_t x_ref;
    int32_t y_ref;
    int32_t x_pos;
    int32_t y_pos;
};

struct gba_lcd_t
{
    uint16_t dispcnt;
    uint16_t dispstat;
    uint16_t vcount;

    lcd_background_t bg[4];

    uint16_t winh[2];
    uint16_t winv[2];
    uint16_t winin;
    uint16_t winout;

    // BG mosaic size
    uint8_t bg_mosaic_hsize;
    uint8_t bg_mosaic_vsize;
    
    // OBJ mosaic size
    uint8_t obj_mosaic_hsize;
    uint8_t obj_mosaic_vsize;

    uint16_t bldcnt;
    uint16_t bldalpha;
    uint16_t bldy;
};

struct lcd_renderer_t
{
    int mode;
    int type;
};

//#define SPRITE_DEBUG

void gfxDrawTextScreen(int);
void gfxDrawRotScreen(int);
void gfxDrawRotScreen16Bit();
void gfxDrawRotScreen256();
void gfxDrawRotScreen16Bit160();
void gfxDrawSprites();
void gfxDrawOBJWin();
static void gfxIncreaseBrightness(uint32_t* line, int coeff);
static void gfxDecreaseBrightness(uint32_t* line, int coeff);
static void gfxAlphaBlend(uint32_t* ta, uint32_t* tb, int ca, int cb);

void mode0RenderLine(pixFormat*);
void mode0RenderLineNoWindow(pixFormat*);
void mode0RenderLineAll(pixFormat*);

void mode1RenderLine(pixFormat*);
void mode1RenderLineNoWindow(pixFormat*);
void mode1RenderLineAll(pixFormat*);

void mode2RenderLine(pixFormat*);
void mode2RenderLineNoWindow(pixFormat*);
void mode2RenderLineAll(pixFormat*);

void mode3RenderLine(pixFormat*);
void mode3RenderLineNoWindow(pixFormat*);
void mode3RenderLineAll(pixFormat*);

void mode4RenderLine(pixFormat*);
void mode4RenderLineNoWindow(pixFormat*);
void mode4RenderLineAll(pixFormat*);

void mode5RenderLine(pixFormat*);
void mode5RenderLineNoWindow(pixFormat*);
void mode5RenderLineAll(pixFormat*);

void forceBlankLine(pixFormat*);
void gfxDrawScanline(pixFormat*, int, int);

// LCD Background Registers
void LCDUpdateBGCNT(int, uint16_t);
void LCDUpdateBGX_L(int, uint16_t);
void LCDUpdateBGX_H(int, uint16_t);
void LCDUpdateBGY_L(int, uint16_t);
void LCDUpdateBGY_H(int, uint16_t);
void LCDUpdateMOSAIC(uint16_t);
void LCDUpdateWindow0();
void LCDUpdateWindow1();
bool LCDUpdateInWindow(int, uint16_t, uint32_t);
void LCDResetBGRegisters();

// Used for savestate compatibility.
// Reloads lcd BG registers to lcd background registers struct after loading states
void LCDUpdateBGRegisters();

extern int coeff[32];
extern uint32_t lineBG[4][240];
extern uint32_t lineOBJ[240];
extern uint32_t lineOBJWin[240];
extern bool gfxInWin0[240];
extern bool gfxInWin1[240];

extern bool oam_updated;
extern bool oam_obj_updated[128];

extern gba_lcd_t lcd;

#define MAKECOLOR(color) ColorMap[(color) & 0xFFFF]

static INLINE uint32_t gfxIncreaseBrightness(uint32_t color, int coeff)
{
    color &= 0xffff;
    color = ((color << 16) | color) & 0x3E07C1F;

    color = color + (((0x3E07C1F - color) * coeff) >> 4);
    color &= 0x3E07C1F;

    return (color >> 16) | color;
}

static INLINE void gfxIncreaseBrightness(uint32_t* line, int coeff)
{
    for (int x = 0; x < 240; x++) {
        uint32_t color = *line;
        int r = (color & 0x1F);
        int g = ((color >> 5) & 0x1F);
        int b = ((color >> 10) & 0x1F);

        r = r + (((31 - r) * coeff) >> 4);
        g = g + (((31 - g) * coeff) >> 4);
        b = b + (((31 - b) * coeff) >> 4);
        if (r > 31)
            r = 31;
        if (g > 31)
            g = 31;
        if (b > 31)
            b = 31;
        *line++ = (color & 0xFFFF0000) | (b << 10) | (g << 5) | r;
    }
}

static INLINE uint32_t gfxDecreaseBrightness(uint32_t color, int coeff)
{
    color &= 0xffff;
    color = ((color << 16) | color) & 0x3E07C1F;

    color = color - (((color * coeff) >> 4) & 0x3E07C1F);

    return (color >> 16) | color;
}

static INLINE void gfxDecreaseBrightness(uint32_t* line, int coeff)
{
    for (int x = 0; x < 240; x++) {
        uint32_t color = *line;
        int r = (color & 0x1F);
        int g = ((color >> 5) & 0x1F);
        int b = ((color >> 10) & 0x1F);

        r = r - ((r * coeff) >> 4);
        g = g - ((g * coeff) >> 4);
        b = b - ((b * coeff) >> 4);
        if (r < 0)
            r = 0;
        if (g < 0)
            g = 0;
        if (b < 0)
            b = 0;
        *line++ = (color & 0xFFFF0000) | (b << 10) | (g << 5) | r;
    }
}

static INLINE uint32_t gfxAlphaBlend(uint32_t color, uint32_t color2, int ca, int cb)
{
    if (color < 0x80000000) {
        color &= 0xffff;
        color2 &= 0xffff;

        color = ((color << 16) | color) & 0x03E07C1F;
        color2 = ((color2 << 16) | color2) & 0x03E07C1F;
        color = ((color * ca) + (color2 * cb)) >> 4;

        if ((ca + cb) > 16) {
            if (color & 0x20)
                color |= 0x1f;
            if (color & 0x8000)
                color |= 0x7C00;
            if (color & 0x4000000)
                color |= 0x03E00000;
        }

        color &= 0x03E07C1F;
        color = (color >> 16) | color;
    }
    return color;
}

static INLINE void gfxAlphaBlend(uint32_t* ta, uint32_t* tb, int ca, int cb)
{
    for (int x = 0; x < 240; x++) {
        uint32_t color = *ta;
        if (color < 0x80000000) {
            int r = (color & 0x1F);
            int g = ((color >> 5) & 0x1F);
            int b = ((color >> 10) & 0x1F);
            uint32_t color2 = (*tb++);
            int r0 = (color2 & 0x1F);
            int g0 = ((color2 >> 5) & 0x1F);
            int b0 = ((color2 >> 10) & 0x1F);

            r = ((r * ca) + (r0 * cb)) >> 4;
            g = ((g * ca) + (g0 * cb)) >> 4;
            b = ((b * ca) + (b0 * cb)) >> 4;

            if (r > 31)
                r = 31;
            if (g > 31)
                g = 31;
            if (b > 31)
                b = 31;

            *ta++ = (color & 0xFFFF0000) | (b << 10) | (g << 5) | r;
        } else {
            ta++;
            tb++;
        }
    }
}

#endif // GFX_H
