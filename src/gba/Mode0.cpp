#include "GBA.h"
#include "GBAGfx.h"
#include "Globals.h"

void mode0RenderLine(pixFormat* lineMix)
{
    uint16_t* palette = (uint16_t*)paletteRAM;

    if (layerEnable & 0x0100) {
        gfxDrawTextScreen(&lcd_bg[0], palette, line0);
    }

    if (layerEnable & 0x0200) {
        gfxDrawTextScreen(&lcd_bg[1], palette, line1);
    }

    if (layerEnable & 0x0400) {
        gfxDrawTextScreen(&lcd_bg[2], palette, line2);
    }

    if (layerEnable & 0x0800) {
        gfxDrawTextScreen(&lcd_bg[3], palette, line3);
    }

    gfxDrawSprites();

    uint32_t backdrop = (READ16LE(&palette[0]) | 0x30000000);

    for (int x = 0; x < 240; x++) {
        uint32_t color = backdrop;
        uint8_t top = 0x20;

        if (line0[x] < color) {
            color = line0[x];
            top = 0x01;
        }

        if ((line1[x] & 0xFF000000) < (color & 0xFF000000)) {
            color = line1[x];
            top = 0x02;
        }

        if ((line2[x] & 0xFF000000) < (color & 0xFF000000)) {
            color = line2[x];
            top = 0x04;
        }

        if ((line3[x] & 0xFF000000) < (color & 0xFF000000)) {
            color = line3[x];
            top = 0x08;
        }

        if ((lineOBJ[x] & 0xFF000000) < (color & 0xFF000000)) {
            color = lineOBJ[x];
            top = 0x10;
        }

        if ((top & 0x10) && (color & 0x00010000)) {
            // semi-transparent OBJ
            uint32_t back = backdrop;
            uint8_t top2 = 0x20;

            if ((line0[x] & 0xFF000000) < (back & 0xFF000000)) {
                back = line0[x];
                top2 = 0x01;
            }

            if ((line1[x] & 0xFF000000) < (back & 0xFF000000)) {
                back = line1[x];
                top2 = 0x02;
            }

            if ((line2[x] & 0xFF000000) < (back & 0xFF000000)) {
                back = line2[x];
                top2 = 0x04;
            }

            if ((line3[x] & 0xFF000000) < (back & 0xFF000000)) {
                back = line3[x];
                top2 = 0x08;
            }

            if (top2 & (BLDMOD >> 8))
                color = gfxAlphaBlend(color, back,
                    coeff[COLEV & 0x1F],
                    coeff[(COLEV >> 8) & 0x1F]);
            else {
                switch ((BLDMOD >> 6) & 3) {
                case 2:
                    if (BLDMOD & top)
                        color = gfxIncreaseBrightness(color, coeff[COLY & 0x1F]);
                    break;
                case 3:
                    if (BLDMOD & top)
                        color = gfxDecreaseBrightness(color, coeff[COLY & 0x1F]);
                    break;
                }
            }
        }

        lineMix[x] = MAKECOLOR(color);
    }
}

void mode0RenderLineNoWindow(pixFormat* lineMix)
{
    uint16_t* palette = (uint16_t*)paletteRAM;

    if (layerEnable & 0x0100) {
        gfxDrawTextScreen(&lcd_bg[0], palette, line0);
    }

    if (layerEnable & 0x0200) {
        gfxDrawTextScreen(&lcd_bg[1], palette, line1);
    }

    if (layerEnable & 0x0400) {
        gfxDrawTextScreen(&lcd_bg[2], palette, line2);
    }

    if (layerEnable & 0x0800) {
        gfxDrawTextScreen(&lcd_bg[3], palette, line3);
    }

    gfxDrawSprites();

    uint32_t backdrop = (READ16LE(&palette[0]) | 0x30000000);

    int effect = (BLDMOD >> 6) & 3;

    for (int x = 0; x < 240; x++) {
        uint32_t color = backdrop;
        uint8_t top = 0x20;

        if (line0[x] < color) {
            color = line0[x];
            top = 0x01;
        }

        if (line1[x] < (color & 0xFF000000)) {
            color = line1[x];
            top = 0x02;
        }

        if (line2[x] < (color & 0xFF000000)) {
            color = line2[x];
            top = 0x04;
        }

        if (line3[x] < (color & 0xFF000000)) {
            color = line3[x];
            top = 0x08;
        }

        if (lineOBJ[x] < (color & 0xFF000000)) {
            color = lineOBJ[x];
            top = 0x10;
        }

        if (!(color & 0x00010000)) {
            switch (effect) {
            case 0:
                break;
            case 1: {
                if (top & BLDMOD) {
                    uint32_t back = backdrop;
                    uint8_t top2 = 0x20;
                    if (line0[x] < back) {
                        if (top != 0x01) {
                            back = line0[x];
                            top2 = 0x01;
                        }
                    }

                    if (line1[x] < (back & 0xFF000000)) {
                        if (top != 0x02) {
                            back = line1[x];
                            top2 = 0x02;
                        }
                    }

                    if (line2[x] < (back & 0xFF000000)) {
                        if (top != 0x04) {
                            back = line2[x];
                            top2 = 0x04;
                        }
                    }

                    if (line3[x] < (back & 0xFF000000)) {
                        if (top != 0x08) {
                            back = line3[x];
                            top2 = 0x08;
                        }
                    }

                    if (lineOBJ[x] < (back & 0xFF000000)) {
                        if (top != 0x10) {
                            back = lineOBJ[x];
                            top2 = 0x10;
                        }
                    }

                    if (top2 & (BLDMOD >> 8))
                        color = gfxAlphaBlend(color, back,
                            coeff[COLEV & 0x1F],
                            coeff[(COLEV >> 8) & 0x1F]);
                }
            } break;
            case 2:
                if (BLDMOD & top)
                    color = gfxIncreaseBrightness(color, coeff[COLY & 0x1F]);
                break;
            case 3:
                if (BLDMOD & top)
                    color = gfxDecreaseBrightness(color, coeff[COLY & 0x1F]);
                break;
            }
        } else {
            // semi-transparent OBJ
            uint32_t back = backdrop;
            uint8_t top2 = 0x20;

            if (line0[x] < back) {
                back = line0[x];
                top2 = 0x01;
            }

            if (line1[x] < (back & 0xFF000000)) {
                back = line1[x];
                top2 = 0x02;
            }

            if (line2[x] < (back & 0xFF000000)) {
                back = line2[x];
                top2 = 0x04;
            }

            if (line3[x] < (back & 0xFF000000)) {
                back = line3[x];
                top2 = 0x08;
            }

            if (top2 & (BLDMOD >> 8))
                color = gfxAlphaBlend(color, back,
                    coeff[COLEV & 0x1F],
                    coeff[(COLEV >> 8) & 0x1F]);
            else {
                switch ((BLDMOD >> 6) & 3) {
                case 2:
                    if (BLDMOD & top)
                        color = gfxIncreaseBrightness(color, coeff[COLY & 0x1F]);
                    break;
                case 3:
                    if (BLDMOD & top)
                        color = gfxDecreaseBrightness(color, coeff[COLY & 0x1F]);
                    break;
                }
            }
        }

        lineMix[x] = MAKECOLOR(color);
    }
}

void mode0RenderLineAll(pixFormat* lineMix)
{
    uint16_t* palette = (uint16_t*)paletteRAM;

    bool inWindow0 = false;
    bool inWindow1 = false;

    if (layerEnable & 0x2000) {
        uint8_t v0 = WIN0V >> 8;
        uint8_t v1 = WIN0V & 255;
        inWindow0 = ((v0 == v1) && (v0 >= 0xe8));
        if (v1 >= v0)
            inWindow0 |= (VCOUNT >= v0 && VCOUNT < v1);
        else
            inWindow0 |= (VCOUNT >= v0 || VCOUNT < v1);
    }
    if (layerEnable & 0x4000) {
        uint8_t v0 = WIN1V >> 8;
        uint8_t v1 = WIN1V & 255;
        inWindow1 = ((v0 == v1) && (v0 >= 0xe8));
        if (v1 >= v0)
            inWindow1 |= (VCOUNT >= v0 && VCOUNT < v1);
        else
            inWindow1 |= (VCOUNT >= v0 || VCOUNT < v1);
    }

    if ((layerEnable & 0x0100)) {
        gfxDrawTextScreen(&lcd_bg[0], palette, line0);
    }

    if ((layerEnable & 0x0200)) {
        gfxDrawTextScreen(&lcd_bg[1], palette, line1);
    }

    if ((layerEnable & 0x0400)) {
        gfxDrawTextScreen(&lcd_bg[2], palette, line2);
    }

    if ((layerEnable & 0x0800)) {
        gfxDrawTextScreen(&lcd_bg[3], palette, line3);
    }

    gfxDrawSprites();
    gfxDrawOBJWin();

    uint32_t backdrop = (READ16LE(&palette[0]) | 0x30000000);

    uint8_t inWin0Mask = WININ & 0xFF;
    uint8_t inWin1Mask = WININ >> 8;
    uint8_t outMask = WINOUT & 0xFF;

    for (int x = 0; x < 240; x++) {
        uint32_t color = backdrop;
        uint8_t top = 0x20;
        uint8_t mask = outMask;

        if (!(lineOBJWin[x] & 0x80000000)) {
            mask = WINOUT >> 8;
        }

        if (inWindow1) {
            if (gfxInWin1[x])
                mask = inWin1Mask;
        }

        if (inWindow0) {
            if (gfxInWin0[x]) {
                mask = inWin0Mask;
            }
        }

        if ((mask & 1) && (line0[x] < color)) {
            color = line0[x];
            top = 0x01;
        }

        if ((mask & 2) && ((line1[x] & 0xFF000000) < (color & 0xFF000000))) {
            color = line1[x];
            top = 0x02;
        }

        if ((mask & 4) && ((line2[x] & 0xFF000000) < (color & 0xFF000000))) {
            color = line2[x];
            top = 0x04;
        }

        if ((mask & 8) && ((line3[x] & 0xFF000000) < (color & 0xFF000000))) {
            color = line3[x];
            top = 0x08;
        }

        if ((mask & 16) && ((lineOBJ[x] & 0xFF000000) < (color & 0xFF000000))) {
            color = lineOBJ[x];
            top = 0x10;
        }

        if (color & 0x00010000) {
            // semi-transparent OBJ
            uint32_t back = backdrop;
            uint8_t top2 = 0x20;

            if ((mask & 1) && ((line0[x] & 0xFF000000) < (back & 0xFF000000))) {
                back = line0[x];
                top2 = 0x01;
            }

            if ((mask & 2) && ((line1[x] & 0xFF000000) < (back & 0xFF000000))) {
                back = line1[x];
                top2 = 0x02;
            }

            if ((mask & 4) && ((line2[x] & 0xFF000000) < (back & 0xFF000000))) {
                back = line2[x];
                top2 = 0x04;
            }

            if ((mask & 8) && ((line3[x] & 0xFF000000) < (back & 0xFF000000))) {
                back = line3[x];
                top2 = 0x08;
            }

            if (top2 & (BLDMOD >> 8))
                color = gfxAlphaBlend(color, back,
                    coeff[COLEV & 0x1F],
                    coeff[(COLEV >> 8) & 0x1F]);
            else {
                switch ((BLDMOD >> 6) & 3) {
                case 2:
                    if (BLDMOD & top)
                        color = gfxIncreaseBrightness(color, coeff[COLY & 0x1F]);
                    break;
                case 3:
                    if (BLDMOD & top)
                        color = gfxDecreaseBrightness(color, coeff[COLY & 0x1F]);
                    break;
                }
            }
        } else if (mask & 32) {
            // special FX on in the window
            switch ((BLDMOD >> 6) & 3) {
            case 0:
                break;
            case 1: {
                if (top & BLDMOD) {
                    uint32_t back = backdrop;
                    uint8_t top2 = 0x20;
                    if ((mask & 1) && (line0[x] & 0xFF000000) < (back & 0xFF000000)) {
                        if (top != 0x01) {
                            back = line0[x];
                            top2 = 0x01;
                        }
                    }

                    if ((mask & 2) && (line1[x] & 0xFF000000) < (back & 0xFF000000)) {
                        if (top != 0x02) {
                            back = line1[x];
                            top2 = 0x02;
                        }
                    }

                    if ((mask & 4) && (line2[x] & 0xFF000000) < (back & 0xFF000000)) {
                        if (top != 0x04) {
                            back = line2[x];
                            top2 = 0x04;
                        }
                    }

                    if ((mask & 8) && (line3[x] & 0xFF000000) < (back & 0xFF000000)) {
                        if (top != 0x08) {
                            back = line3[x];
                            top2 = 0x08;
                        }
                    }

                    if ((mask & 16) && (lineOBJ[x] & 0xFF000000) < (back & 0xFF000000)) {
                        if (top != 0x10) {
                            back = lineOBJ[x];
                            top2 = 0x10;
                        }
                    }

                    if (top2 & (BLDMOD >> 8))
                        color = gfxAlphaBlend(color, back,
                            coeff[COLEV & 0x1F],
                            coeff[(COLEV >> 8) & 0x1F]);
                }
            } break;
            case 2:
                if (BLDMOD & top)
                    color = gfxIncreaseBrightness(color, coeff[COLY & 0x1F]);
                break;
            case 3:
                if (BLDMOD & top)
                    color = gfxDecreaseBrightness(color, coeff[COLY & 0x1F]);
                break;
            }
        }

        lineMix[x] = MAKECOLOR(color);
    }
}
