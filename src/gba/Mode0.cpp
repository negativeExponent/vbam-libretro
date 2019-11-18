#include "GBAGfx.h"

void mode0RenderLine(pixFormat* lineMix)
{
    uint16_t* palette = (uint16_t*)paletteRAM;
    uint32_t backdrop = (READ16LE(&palette[0]) | 0x30000000);

    for (int x = 0; x < 240; x++) {
        uint32_t color = backdrop;
        uint8_t top = 0x20;

        if (lineBG[0][x] < color) {
            color = lineBG[0][x];
            top = 0x01;
        }

        if ((lineBG[1][x] & 0xFF000000) < (color & 0xFF000000)) {
            color = lineBG[1][x];
            top = 0x02;
        }

        if ((lineBG[2][x] & 0xFF000000) < (color & 0xFF000000)) {
            color = lineBG[2][x];
            top = 0x04;
        }

        if ((lineBG[3][x] & 0xFF000000) < (color & 0xFF000000)) {
            color = lineBG[3][x];
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

            if ((lineBG[0][x] & 0xFF000000) < (back & 0xFF000000)) {
                back = lineBG[0][x];
                top2 = 0x01;
            }

            if ((lineBG[1][x] & 0xFF000000) < (back & 0xFF000000)) {
                back = lineBG[1][x];
                top2 = 0x02;
            }

            if ((lineBG[2][x] & 0xFF000000) < (back & 0xFF000000)) {
                back = lineBG[2][x];
                top2 = 0x04;
            }

            if ((lineBG[3][x] & 0xFF000000) < (back & 0xFF000000)) {
                back = lineBG[3][x];
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

    uint32_t backdrop = (READ16LE(&palette[0]) | 0x30000000);

    int effect = (BLDMOD >> 6) & 3;

    for (int x = 0; x < 240; x++) {
        uint32_t color = backdrop;
        uint8_t top = 0x20;

        if (lineBG[0][x] < color) {
            color = lineBG[0][x];
            top = 0x01;
        }

        if (lineBG[1][x] < (color & 0xFF000000)) {
            color = lineBG[1][x];
            top = 0x02;
        }

        if (lineBG[2][x] < (color & 0xFF000000)) {
            color = lineBG[2][x];
            top = 0x04;
        }

        if (lineBG[3][x] < (color & 0xFF000000)) {
            color = lineBG[3][x];
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
                    if (lineBG[0][x] < back) {
                        if (top != 0x01) {
                            back = lineBG[0][x];
                            top2 = 0x01;
                        }
                    }

                    if (lineBG[1][x] < (back & 0xFF000000)) {
                        if (top != 0x02) {
                            back = lineBG[1][x];
                            top2 = 0x02;
                        }
                    }

                    if (lineBG[2][x] < (back & 0xFF000000)) {
                        if (top != 0x04) {
                            back = lineBG[2][x];
                            top2 = 0x04;
                        }
                    }

                    if (lineBG[3][x] < (back & 0xFF000000)) {
                        if (top != 0x08) {
                            back = lineBG[3][x];
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

            if (lineBG[0][x] < back) {
                back = lineBG[0][x];
                top2 = 0x01;
            }

            if (lineBG[1][x] < (back & 0xFF000000)) {
                back = lineBG[1][x];
                top2 = 0x02;
            }

            if (lineBG[2][x] < (back & 0xFF000000)) {
                back = lineBG[2][x];
                top2 = 0x04;
            }

            if (lineBG[3][x] < (back & 0xFF000000)) {
                back = lineBG[3][x];
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

    bool inWindow0 = LCDPossibleInWindow(0, WIN0V, VCOUNT);
    bool inWindow1 = LCDPossibleInWindow(1, WIN1V, VCOUNT);

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

        if ((mask & 1) && (lineBG[0][x] < color)) {
            color = lineBG[0][x];
            top = 0x01;
        }

        if ((mask & 2) && ((lineBG[1][x] & 0xFF000000) < (color & 0xFF000000))) {
            color = lineBG[1][x];
            top = 0x02;
        }

        if ((mask & 4) && ((lineBG[2][x] & 0xFF000000) < (color & 0xFF000000))) {
            color = lineBG[2][x];
            top = 0x04;
        }

        if ((mask & 8) && ((lineBG[3][x] & 0xFF000000) < (color & 0xFF000000))) {
            color = lineBG[3][x];
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

            if ((mask & 1) && ((lineBG[0][x] & 0xFF000000) < (back & 0xFF000000))) {
                back = lineBG[0][x];
                top2 = 0x01;
            }

            if ((mask & 2) && ((lineBG[1][x] & 0xFF000000) < (back & 0xFF000000))) {
                back = lineBG[1][x];
                top2 = 0x02;
            }

            if ((mask & 4) && ((lineBG[2][x] & 0xFF000000) < (back & 0xFF000000))) {
                back = lineBG[2][x];
                top2 = 0x04;
            }

            if ((mask & 8) && ((lineBG[3][x] & 0xFF000000) < (back & 0xFF000000))) {
                back = lineBG[3][x];
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
                    if ((mask & 1) && (lineBG[0][x] & 0xFF000000) < (back & 0xFF000000)) {
                        if (top != 0x01) {
                            back = lineBG[0][x];
                            top2 = 0x01;
                        }
                    }

                    if ((mask & 2) && (lineBG[1][x] & 0xFF000000) < (back & 0xFF000000)) {
                        if (top != 0x02) {
                            back = lineBG[1][x];
                            top2 = 0x02;
                        }
                    }

                    if ((mask & 4) && (lineBG[2][x] & 0xFF000000) < (back & 0xFF000000)) {
                        if (top != 0x04) {
                            back = lineBG[2][x];
                            top2 = 0x04;
                        }
                    }

                    if ((mask & 8) && (lineBG[3][x] & 0xFF000000) < (back & 0xFF000000)) {
                        if (top != 0x08) {
                            back = lineBG[3][x];
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
