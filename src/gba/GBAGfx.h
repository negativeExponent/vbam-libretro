#ifndef GFX_H
#define GFX_H

#include "GBA.h"
#include "Globals.h"
#include "../common/Port.h"

typedef struct 
{
    uint16_t control;
    uint16_t h_offset;
    uint16_t v_offset;
    int dx;
    int dmx;
    int dy;
    int dmy;
    int x_ref;
    int y_ref;
    int x_pos;
    int y_pos;
} lcd_background_t;

typedef struct
{
    int mode;
    int type;
} lcd_renderer_t;

//#define SPRITE_DEBUG

void gfxDrawTextScreen(lcd_background_t*, uint16_t*, uint32_t*);
void gfxDrawRotScreen(lcd_background_t*, uint16_t*, uint32_t*);
void gfxDrawRotScreen16Bit(lcd_background_t*);
void gfxDrawRotScreen256(lcd_background_t*);
void gfxDrawRotScreen16Bit160(lcd_background_t*);
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

void forceBlankLine(pixFormat* lineMix);

// LCD Background Registers
void LCDUpdateBGCNT(lcd_background_t* bg, uint16_t value);
void LCDUpdateBGHOFS(lcd_background_t* bg, uint16_t value);
void LCDUpdateBGVOFS(lcd_background_t* bg, uint16_t value);
void LCDUpdateBGPA(lcd_background_t* bg, uint16_t value);
void LCDUpdateBGPB(lcd_background_t* bg, uint16_t value);
void LCDUpdateBGPC(lcd_background_t* bg, uint16_t value);
void LCDUpdateBGPD(lcd_background_t* bg, uint16_t value);
void LCDUpdateBGX_L(lcd_background_t* bg, uint16_t value);
void LCDUpdateBGX_H(lcd_background_t* bg, uint16_t value);
void LCDUpdateBGY_L(lcd_background_t* bg, uint16_t value);
void LCDUpdateBGY_H(lcd_background_t* bg, uint16_t value);
void LCDUpdateWindow0();
void LCDUpdateWindow1();
void LCDUpdateBGRef(int);
void LCDResetBGParams();

// Used for savestate compatibility.
// Reloads lcd io BG regs to lcd background registers after loading states
void LCDLoadStateBGParams();

extern int coeff[32];
extern uint32_t line0[240];
extern uint32_t line1[240];
extern uint32_t line2[240];
extern uint32_t line3[240];
extern uint32_t lineOBJ[240];
extern uint32_t lineOBJWin[240];
extern bool gfxInWin0[240];
extern bool gfxInWin1[240];
extern int lineOBJpixleft[128];
extern lcd_background_t lcd_bg[4];
extern uint16_t systemColorMap16[0x10000];

#define MAKECOLOR(color) systemColorMap16[(color) & 0xFFFF]

static inline uint32_t gfxIncreaseBrightness(uint32_t color, int coeff)
{
    color &= 0xffff;
    color = ((color << 16) | color) & 0x3E07C1F;

    color = color + (((0x3E07C1F - color) * coeff) >> 4);
    color &= 0x3E07C1F;

    return (color >> 16) | color;
}

static inline void gfxIncreaseBrightness(uint32_t* line, int coeff)
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

static inline uint32_t gfxDecreaseBrightness(uint32_t color, int coeff)
{
    color &= 0xffff;
    color = ((color << 16) | color) & 0x3E07C1F;

    color = color - (((color * coeff) >> 4) & 0x3E07C1F);

    return (color >> 16) | color;
}

static inline void gfxDecreaseBrightness(uint32_t* line, int coeff)
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

static inline uint32_t gfxAlphaBlend(uint32_t color, uint32_t color2, int ca, int cb)
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

static inline void gfxAlphaBlend(uint32_t* ta, uint32_t* tb, int ca, int cb)
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
