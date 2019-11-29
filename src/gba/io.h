#ifndef IO_H
#define IO_H

enum io_registers
{
    REG_DISPCNT     = 0x000,
    REG_GREENSWAP   = 0x002,
    REG_DISPSTAT    = 0x004,
    REG_VCOUNT      = 0x006,
    REG_BG0CNT      = 0x008,
    REG_BG1CNT      = 0x00A,
    REG_BG2CNT      = 0x00C,
    REG_BG3CNT      = 0x00E,
    REG_BG0HOFS     = 0x010,
    REG_BG0VOFS     = 0x012,
    REG_BG1HOFS     = 0x014,
    REG_BG1VOFS     = 0x016,
    REG_BG2HOFS     = 0x018,
    REG_BG2VOFS     = 0x01A,
    REG_BG3HOFS     = 0x01C,
    REG_BG3VOFS     = 0x01E,
    REG_BG2PA       = 0x020,
    REG_BG2PB       = 0x022,
    REG_BG2PC       = 0x024,
    REG_BG2PD       = 0x026,
    REG_BG2X_L      = 0x028,
    REG_BG2X_H      = 0x02A,
    REG_BG2Y_L      = 0x02C,
    REG_BG2Y_H      = 0x02E,
    REG_BG3PA       = 0x030,
    REG_BG3PB       = 0x032,
    REG_BG3PC       = 0x034,
    REG_BG3PD       = 0x036,
    REG_BG3X_L      = 0x038,
    REG_BG3X_H      = 0x03A,
    REG_BG3Y_L      = 0x03C,
    REG_BG3Y_H      = 0x03E,
    REG_WIN0H       = 0x040,
    REG_WIN1H       = 0x042,
    REG_WIN0V       = 0x044,
    REG_WIN1V       = 0x046,
    REG_WININ       = 0x048,
    REG_WINOUT      = 0x04A,
    REG_MOSAIC      = 0x04C,
    REG_BLDCNT      = 0x050,
    REG_BLDALPHA    = 0x052,
    REG_BLDY        = 0x054,
    REG_SOUND1CNT_L = 0x060,
    REG_SOUND1CNT_H = 0x062,
    REG_SOUND1CNT_X = 0x064,
    REG_SOUND2CNT_L = 0x068,
    REG_SOUND2CNT_H = 0x06C,
    REG_SOUND3CNT_L = 0x070,
    REG_SOUND3CNT_H = 0x072,
    REG_SOUND3CNT_X = 0x074,
    REG_SOUND4CNT_L = 0x078,
    REG_SOUND4CNT_H = 0x07C,
    REG_SOUNDCNT_L  = 0x080,
    REG_SOUNDCNT_H  = 0x082,
    REG_SOUNDCNT_X  = 0x084,
    REG_SOUNDBIAS   = 0x088,
    REG_WAVE_RAM    = 0x090,
    REG_FIFO_A_L    = 0x0A0,
    REG_FIFO_A_H    = 0x0A2,
    REG_FIFO_B_L    = 0x0A4,
    REG_FIFO_B_H    = 0x0A6,
    REG_DMA0SAD_L   = 0x0B0,
    REG_DMA0SAD_H   = 0x0B2,
    REG_DMA0DAD_L   = 0x0B4,
    REG_DMA0DAD_H   = 0x0B6,
    REG_DMA0CNT_L   = 0x0B8,
    REG_DMA0CNT_H   = 0x0BA,
    REG_DMA1SAD_L   = 0x0BC,
    REG_DMA1SAD_H   = 0x0BE,
    REG_DMA1DAD_L   = 0x0C0,
    REG_DMA1DAD_H   = 0x0C2,
    REG_DMA1CNT_L   = 0x0C4,
    REG_DMA1CNT_H   = 0x0C6,
    REG_DMA2SAD_L   = 0x0C8,
    REG_DMA2SAD_H   = 0x0CA,
    REG_DMA2DAD_L   = 0x0CC,
    REG_DMA2DAD_H   = 0x0CE,
    REG_DMA2CNT_L   = 0x0D0,
    REG_DMA2CNT_H   = 0x0D2,
    REG_DMA3SAD_L   = 0x0D4,
    REG_DMA3SAD_H   = 0x0D6,
    REG_DMA3DAD_L   = 0x0D8,
    REG_DMA3DAD_H   = 0x0DA,
    REG_DMA3CNT_L   = 0x0DC,
    REG_DMA3CNT_H   = 0x0DE,
    REG_TM0CNT_L    = 0x100,
    REG_TM0CNT_H    = 0x102,
    REG_TM1CNT_L    = 0x104,
    REG_TM1CNT_H    = 0x106,
    REG_TM2CNT_L    = 0x108,
    REG_TM2CNT_H    = 0x10A,
    REG_TM3CNT_L    = 0x10C,
    REG_TM3CNT_H    = 0x10E,
    REG_KEYINPUT    = 0x130,
    REG_KEYCNT      = 0x132,
    REG_RCNT        = 0x134,
    REG_JOYCNT      = 0x140,
    REG_JOY_RECV    = 0x150,
    REG_JOY_TRANS   = 0x154,
    REG_JOYSTAT     = 0x158,
    REG_IE          = 0x200,
    REG_IF          = 0x202,
    REG_WAITCNT     = 0x204,
    REG_IME         = 0x208,
    REG_POSTFLG     = 0x300,
    REG_HALTCNT     = 0x301
};

#endif // IO_H
