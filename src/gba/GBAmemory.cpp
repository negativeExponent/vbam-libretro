#include <string.h>

#include "GBAGfx.h"
#include "GBALink.h"
#include "GBAinline.h"

extern int layerEnableDelay;
extern bool windowOn;
extern bool fxOn;
static const uint32_t objTilesAddress[3] = { 0x010000, 0x014000, 0x014000 };
static const uint8_t gamepakRamWaitState[4] = { 4, 3, 2, 8 };
static const uint8_t gamepakWaitState[4] = { 4, 3, 2, 8 };
static const uint8_t gamepakWaitState0[2] = { 2, 1 };
static const uint8_t gamepakWaitState1[2] = { 4, 1 };
static const uint8_t gamepakWaitState2[2] = { 8, 1 };

void CPUUpdateRegister(uint32_t address, uint16_t value)
{
   /*if (address >= 0x008 && address < 0x056)
   {
      log("LCD REG: A:%04x V:%04x VideoMode = %02x VCOUNT = %3d\n",
         address, value, lcd.dispcnt & 7, lcd.vcount);
   }*/

    switch (address) {
    case 0x00: {
        if ((value & 7) <= 5) {
            // display modes above 0-5 are prohibited
            lcd.dispcnt = (value & 7);
        } else log("Invalid display mode: %d\n",value & 7);
        bool change = (0 != ((lcd.dispcnt ^ value) & 0x80));
        bool changeBG = (0 != ((lcd.dispcnt ^ value) & 0x0F00));
        uint16_t changeBGon = ((~lcd.dispcnt) & value) & 0x0F00; // these layers are being activated

        lcd.dispcnt = (value & 0xFFF7); // bit 3 can only be accessed by the BIOS to enable GBC mode
        UPDATE_REG(0x00, lcd.dispcnt);

        if (changeBGon) {
            layerEnableDelay = 4;
            layerEnable = layerSettings & value & (~changeBGon);
        } else {
            layerEnable = layerSettings & value;
            // CPUUpdateTicks();
        }

        windowOn = (layerEnable & 0x6000) ? true : false;
        if (change && !((value & 0x80))) {
            if (!(lcd.dispstat & 1)) {
                lcd.dispstat &= 0xFFFC;
                UPDATE_REG(0x04, lcd.dispstat);
                CPUCompareVCOUNT();
            }
        }
        CPUUpdateRender();
        // we only care about changes in BG0-BG3
        if (changeBG) {
            CPUUpdateRenderBuffers(false);
        }
        break;
    }
    case 0x04:
        lcd.dispstat = (value & 0xFF38) | (lcd.dispstat & 7);
        UPDATE_REG(0x04, lcd.dispstat);
        break;
    case 0x06:
        // lcd.vcount, not writable
        break;
    case 0x08:
        BG0CNT = (value & 0xDFFF);
        UPDATE_REG(0x08, BG0CNT);
        LCDUpdateBGCNT(0, BG0CNT);
        break;
    case 0x0A:
        BG1CNT = (value & 0xDFFF);
        UPDATE_REG(0x0A, BG1CNT);
        LCDUpdateBGCNT(1, BG1CNT);
        break;
    case 0x0C:
        BG2CNT = (value & 0xFFFF);
        UPDATE_REG(0x0C, BG2CNT);
        LCDUpdateBGCNT(2, BG2CNT);
        break;
    case 0x0E:
        BG3CNT = (value & 0xFFFF);
        UPDATE_REG(0x0E, BG3CNT);
        LCDUpdateBGCNT(3, BG3CNT);
        break;
    case 0x10:
        lcd.bg[0].hofs = value & 511;
        UPDATE_REG(0x10, lcd.bg[0].hofs);
        break;
    case 0x12:
        lcd.bg[0].vofs = value & 511;
        UPDATE_REG(0x12, lcd.bg[0].vofs);
        break;
    case 0x14:
        lcd.bg[1].hofs = value & 511;
        UPDATE_REG(0x14, lcd.bg[1].hofs);
        break;
    case 0x16:
        lcd.bg[1].vofs = value & 511;
        UPDATE_REG(0x16, lcd.bg[1].vofs);
        break;
    case 0x18:
        lcd.bg[2].hofs = value & 511;
        UPDATE_REG(0x18, lcd.bg[2].hofs);
        break;
    case 0x1A:
        lcd.bg[2].vofs = value & 511;
        UPDATE_REG(0x1A, lcd.bg[2].vofs);
        break;
    case 0x1C:
        lcd.bg[3].hofs = value & 511;
        UPDATE_REG(0x1C, lcd.bg[3].hofs);
        break;
    case 0x1E:
        lcd.bg[3].vofs = value & 511;
        UPDATE_REG(0x1E, lcd.bg[3].vofs);
        break;
    case 0x20:
        BG2PA = value;
        lcd.bg[2].dx = value;
        UPDATE_REG(0x20, BG2PA);
        break;
    case 0x22:
        BG2PB = value;
        lcd.bg[2].dmx = value;
        UPDATE_REG(0x22, BG2PB);
        break;
    case 0x24:
        BG2PC = value;
        lcd.bg[2].dy = value;
        UPDATE_REG(0x24, BG2PC);
        break;
    case 0x26:
        BG2PD = value;
        lcd.bg[2].dmy = value;
        UPDATE_REG(0x26, BG2PD);
        break;
    case 0x28:
        BG2X_L = value;
        UPDATE_REG(0x28, BG2X_L);
        LCDUpdateBGX_L(2, BG2X_L);
        break;
    case 0x2A:
        BG2X_H = (value & 0xFFF);
        UPDATE_REG(0x2A, BG2X_H);
        LCDUpdateBGX_H(2, BG2X_H);
        break;
    case 0x2C:
        BG2Y_L = value;
        UPDATE_REG(0x2C, BG2Y_L);
        LCDUpdateBGY_L(2, BG2Y_L);
        break;
    case 0x2E:
        BG2Y_H = value & 0xFFF;
        UPDATE_REG(0x2E, BG2Y_H);
        LCDUpdateBGY_H(2, BG2Y_H);
        break;
    case 0x30:
        BG3PA = value;
        lcd.bg[3].dx = value;
        UPDATE_REG(0x30, BG3PA);
        break;
    case 0x32:
        BG3PB = value;
        lcd.bg[3].dmx = value;
        UPDATE_REG(0x32, BG3PB);
        break;
    case 0x34:
        BG3PC = value;
        lcd.bg[3].dy = value;
        UPDATE_REG(0x34, BG3PC);
        break;
    case 0x36:
        BG3PD = value;
        lcd.bg[3].dmy = value;
        UPDATE_REG(0x36, BG3PD);
        break;
    case 0x38:
        BG3X_L = value;
        UPDATE_REG(0x38, BG3X_L);
        LCDUpdateBGX_L(3, BG3X_L);
        break;
    case 0x3A:
        BG3X_H = value & 0xFFF;
        UPDATE_REG(0x3A, BG3X_H);
        LCDUpdateBGX_H(3, BG3X_H);
        break;
    case 0x3C:
        BG3Y_L = value;
        UPDATE_REG(0x3C, BG3Y_L);
        LCDUpdateBGY_L(3, BG3Y_L);
        break;
    case 0x3E:
        BG3Y_H = value & 0xFFF;
        UPDATE_REG(0x3E, BG3Y_H);
        LCDUpdateBGY_H(3, BG3Y_H);
        break;
    case 0x40:
        lcd.winh[0] = value;
        UPDATE_REG(0x40, lcd.winh[0]);
        LCDUpdateWindow0();
        break;
    case 0x42:
        lcd.winh[1] = value;
        UPDATE_REG(0x42, lcd.winh[1]);
        LCDUpdateWindow1();
        break;
    case 0x44:
        lcd.winv[0] = value;
        UPDATE_REG(0x44, lcd.winv[0]);
        break;
    case 0x46:
        lcd.winv[1] = value;
        UPDATE_REG(0x46, lcd.winv[1]);
        break;
    case 0x48:
        lcd.winin = value & 0x3F3F;
        UPDATE_REG(0x48, lcd.winin);
        break;
    case 0x4A:
        lcd.winout = value & 0x3F3F;
        UPDATE_REG(0x4A, lcd.winout);
        break;
    case 0x4C:
        lcd.mosaic = value;
        UPDATE_REG(0x4C, lcd.mosaic);
        break;
    case 0x50:
        lcd.bldcnt = value & 0x3FFF;
        UPDATE_REG(0x50, lcd.bldcnt);
        fxOn = ((lcd.bldcnt >> 6) & 3) != 0;
        CPUUpdateRender();
        break;
    case 0x52:
        lcd.bldalpha = value & 0x1F1F;
        UPDATE_REG(0x52, lcd.bldalpha);
        break;
    case 0x54:
        lcd.bldy = value & 0x1F;
        UPDATE_REG(0x54, lcd.bldy);
        break;
    case 0x60:
    case 0x62:
    case 0x64:
    case 0x68:
    case 0x6c:
    case 0x70:
    case 0x72:
    case 0x74:
    case 0x78:
    case 0x7c:
    case 0x80:
    case 0x84:
        soundEvent8(address & 0xFF, (uint8_t)(value & 0xFF));
        soundEvent8((address & 0xFF) + 1, (uint8_t)(value >> 8));
        return;
    case 0x82:
    case 0x88:
    case 0xa0:
    case 0xa2:
    case 0xa4:
    case 0xa6:
    case 0x90:
    case 0x92:
    case 0x94:
    case 0x96:
    case 0x98:
    case 0x9a:
    case 0x9c:
    case 0x9e:
        soundEvent16(address & 0xFF, value);
        return;
    case 0xB0:
        DM0SAD_L = value;
        UPDATE_REG(0xB0, DM0SAD_L);
        break;
    case 0xB2:
        DM0SAD_H = value & 0x07FF;
        UPDATE_REG(0xB2, DM0SAD_H);
        break;
    case 0xB4:
        DM0DAD_L = value;
        UPDATE_REG(0xB4, DM0DAD_L);
        break;
    case 0xB6:
        DM0DAD_H = value & 0x07FF;
        UPDATE_REG(0xB6, DM0DAD_H);
        break;
    case 0xB8:
        DM0CNT_L = value & 0x3FFF;
        UPDATE_REG(0xB8, 0);
        break;
    case 0xBA: {
        bool start = ((DM0CNT_H ^ value) & 0x8000) ? true : false;
        value &= 0xF7E0;

        DM0CNT_H = value;
        UPDATE_REG(0xBA, DM0CNT_H);

        if (start && (value & 0x8000)) {
            dma[0].Source = DM0SAD_L | (DM0SAD_H << 16);
            dma[0].Dest = DM0DAD_L | (DM0DAD_H << 16);
            CPUCheckDMA(0, 1);
        }
    } break;
    case 0xBC:
        DM1SAD_L = value;
        UPDATE_REG(0xBC, DM1SAD_L);
        break;
    case 0xBE:
        DM1SAD_H = value & 0x0FFF;
        UPDATE_REG(0xBE, DM1SAD_H);
        break;
    case 0xC0:
        DM1DAD_L = value;
        UPDATE_REG(0xC0, DM1DAD_L);
        break;
    case 0xC2:
        DM1DAD_H = value & 0x07FF;
        UPDATE_REG(0xC2, DM1DAD_H);
        break;
    case 0xC4:
        DM1CNT_L = value & 0x3FFF;
        UPDATE_REG(0xC4, 0);
        break;
    case 0xC6: {
        bool start = ((DM1CNT_H ^ value) & 0x8000) ? true : false;
        value &= 0xF7E0;

        DM1CNT_H = value;
        UPDATE_REG(0xC6, DM1CNT_H);

        if (start && (value & 0x8000)) {
            dma[1].Source = DM1SAD_L | (DM1SAD_H << 16);
            dma[1].Dest = DM1DAD_L | (DM1DAD_H << 16);
            CPUCheckDMA(0, 2);
        }
    } break;
    case 0xC8:
        DM2SAD_L = value;
        UPDATE_REG(0xC8, DM2SAD_L);
        break;
    case 0xCA:
        DM2SAD_H = value & 0x0FFF;
        UPDATE_REG(0xCA, DM2SAD_H);
        break;
    case 0xCC:
        DM2DAD_L = value;
        UPDATE_REG(0xCC, DM2DAD_L);
        break;
    case 0xCE:
        DM2DAD_H = value & 0x07FF;
        UPDATE_REG(0xCE, DM2DAD_H);
        break;
    case 0xD0:
        DM2CNT_L = value & 0x3FFF;
        UPDATE_REG(0xD0, 0);
        break;
    case 0xD2: {
        bool start = ((DM2CNT_H ^ value) & 0x8000) ? true : false;

        value &= 0xF7E0;

        DM2CNT_H = value;
        UPDATE_REG(0xD2, DM2CNT_H);

        if (start && (value & 0x8000)) {
            dma[2].Source = DM2SAD_L | (DM2SAD_H << 16);
            dma[2].Dest = DM2DAD_L | (DM2DAD_H << 16);

            CPUCheckDMA(0, 4);
        }
    } break;
    case 0xD4:
        DM3SAD_L = value;
        UPDATE_REG(0xD4, DM3SAD_L);
        break;
    case 0xD6:
        DM3SAD_H = value & 0x0FFF;
        UPDATE_REG(0xD6, DM3SAD_H);
        break;
    case 0xD8:
        DM3DAD_L = value;
        UPDATE_REG(0xD8, DM3DAD_L);
        break;
    case 0xDA:
        DM3DAD_H = value & 0x0FFF;
        UPDATE_REG(0xDA, DM3DAD_H);
        break;
    case 0xDC:
        DM3CNT_L = value;
        UPDATE_REG(0xDC, 0);
        break;
    case 0xDE: {
        bool start = ((DM3CNT_H ^ value) & 0x8000) ? true : false;

        value &= 0xFFE0;

        DM3CNT_H = value;
        UPDATE_REG(0xDE, DM3CNT_H);

        if (start && (value & 0x8000)) {
            dma[3].Source = DM3SAD_L | (DM3SAD_H << 16);
            dma[3].Dest = DM3DAD_L | (DM3DAD_H << 16);
            CPUCheckDMA(0, 8);
        }
    } break;
    case 0x100:
        timers.tm[0].Reload = value;
        interp_rate();
        break;
    case 0x102:
        timers.tm[0].Value = value;
        timers.OnOffDelay |= 1;
        cpuNextEvent = cpuTotalTicks;
        break;
    case 0x104:
        timers.tm[1].Reload = value;
        interp_rate();
        break;
    case 0x106:
        timers.tm[1].Value = value;
        timers.OnOffDelay |= 2;
        cpuNextEvent = cpuTotalTicks;
        break;
    case 0x108:
        timers.tm[2].Reload = value;
        break;
    case 0x10A:
        timers.tm[2].Value = value;
        timers.OnOffDelay |= 4;
        cpuNextEvent = cpuTotalTicks;
        break;
    case 0x10C:
        timers.tm[3].Reload = value;
        break;
    case 0x10E:
        timers.tm[3].Value = value;
        timers.OnOffDelay |= 8;
        cpuNextEvent = cpuTotalTicks;
        break;

#ifndef NO_LINK
    case COMM_SIOCNT:
        StartLink(value);
        break;

    case COMM_SIODATA8:
        UPDATE_REG(COMM_SIODATA8, value);
        break;

    case COMM_RCNT:
        StartGPLink(value);
        break;

    case COMM_JOYCNT: {
        uint16_t cur = READ16LE(&ioMem[COMM_JOYCNT]);

        if (value & JOYCNT_RESET)
            cur &= ~JOYCNT_RESET;
        if (value & JOYCNT_RECV_COMPLETE)
            cur &= ~JOYCNT_RECV_COMPLETE;
        if (value & JOYCNT_SEND_COMPLETE)
            cur &= ~JOYCNT_SEND_COMPLETE;
        if (value & JOYCNT_INT_ENABLE)
            cur |= JOYCNT_INT_ENABLE;

        UPDATE_REG(COMM_JOYCNT, cur);
    } break;

    case COMM_JOY_RECV_L:
        UPDATE_REG(COMM_JOY_RECV_L, value);
        break;
    case COMM_JOY_RECV_H:
        UPDATE_REG(COMM_JOY_RECV_H, value);
        break;

    case COMM_JOY_TRANS_L:
        UPDATE_REG(COMM_JOY_TRANS_L, value);
        UPDATE_REG(COMM_JOYSTAT, READ16LE(&ioMem[COMM_JOYSTAT]) | JOYSTAT_SEND);
        break;
    case COMM_JOY_TRANS_H:
        UPDATE_REG(COMM_JOY_TRANS_H, value);
        UPDATE_REG(COMM_JOYSTAT, READ16LE(&ioMem[COMM_JOYSTAT]) | JOYSTAT_SEND);
        break;

    case COMM_JOYSTAT:
        UPDATE_REG(COMM_JOYSTAT, (READ16LE(&ioMem[COMM_JOYSTAT]) & 0x0a) | (value & ~0x0a));
        break;
#endif

    case 0x130:
        P1 |= (value & 0x3FF);
        UPDATE_REG(0x130, P1);
        break;

    case 0x132:
        UPDATE_REG(0x132, value & 0xC3FF);
        break;
    case 0x200:
        IE = value & 0x3FFF;
        UPDATE_REG(0x200, IE);
        if ((IME & 1) && (IF & IE) && armIrqEnable)
            cpuNextEvent = cpuTotalTicks;
        break;
    case 0x202:
        IF ^= (value & IF);
        UPDATE_REG(0x202, IF);
        break;
    case 0x204: {
        memoryWait[0x0e] = memoryWaitSeq[0x0e] = gamepakRamWaitState[value & 3];

        memoryWait[0x08] = memoryWait[0x09] = gamepakWaitState[(value >> 2) & 3];
        memoryWaitSeq[0x08] = memoryWaitSeq[0x09] = gamepakWaitState0[(value >> 4) & 1];

        memoryWait[0x0a] = memoryWait[0x0b] = gamepakWaitState[(value >> 5) & 3];
        memoryWaitSeq[0x0a] = memoryWaitSeq[0x0b] = gamepakWaitState1[(value >> 7) & 1];

        memoryWait[0x0c] = memoryWait[0x0d] = gamepakWaitState[(value >> 8) & 3];
        memoryWaitSeq[0x0c] = memoryWaitSeq[0x0d] = gamepakWaitState2[(value >> 10) & 1];

        for (int i = 8; i < 15; i++) {
            memoryWait32[i] = memoryWait[i] + memoryWaitSeq[i] + 1;
            memoryWaitSeq32[i] = memoryWaitSeq[i] * 2 + 1;
        }

        if ((value & 0x4000) == 0x4000) {
            busPrefetchEnable = true;
            busPrefetch = false;
            busPrefetchCount = 0;
        } else {
            busPrefetchEnable = false;
            busPrefetch = false;
            busPrefetchCount = 0;
        }
        UPDATE_REG(0x204, value & 0x7FFF);

    } break;
    case 0x208:
        IME = value & 1;
        UPDATE_REG(0x208, IME);
        if ((IME & 1) && (IF & IE) && armIrqEnable)
            cpuNextEvent = cpuTotalTicks;
        break;
    case 0x300:
        if (value != 0)
            value &= 0xFFFE;
        UPDATE_REG(0x300, value);
        break;
    default:
        UPDATE_REG(address & 0x3FE, value);
        break;
    }
}

static inline uint32_t ROR(uint32_t value, uint32_t shift)
{
   if (!shift)
      return value;
   return ((value >> shift) | (value << (32 - shift)));
}

uint32_t CPUReadMemory(uint32_t address)
{
   uint32_t value = 0;
   switch (address >> 24)
   {
      case 0:
         if (address < 0x4000)
         {
            if (reg[15].I >> 24)
            {
               value = READ32LE((uint32_t*)&biosProtected);
            }
            else
            {
               value = READ32LE(bios + (address & 0x3FFC));
            }
         }
         else
         {
            goto unreadable;
         }
         break;
      case 2:
         value = READ32LE(workRAM + (address & 0x3FFFC));
         break;
      case 3:
         value = READ32LE(internalRAM + (address & 0x7FFC));
         break;
      case 4:
         if ((address < 0x4000400) && ioReadable[address & 0x3FC])
         {
            if (ioReadable[(address & 0x3FC) + 2])
            {
               value = READ32LE(ioMem + (address & 0x3FC));
               if ((address & 0x3FC) == COMM_JOY_RECV_L)
               {
                  UPDATE_REG(COMM_JOYSTAT, READ16LE(&ioMem[COMM_JOYSTAT]) & ~JOYSTAT_RECV);
               }
            }
            else
            {
               value = READ16LE(ioMem + (address & 0x3FC));
            }
         }
         else
         {
            goto unreadable;
         }
         break;
      case 5:
         value = READ32LE(paletteRAM + (address & 0x3FC));
         break;
      case 6:
      {
         uint32_t vram_addr = (address & 0x1FFFC);
         if (((lcd.dispcnt & 7) > 2) && ((address & 0x1C000) == 0x18000))
         {
            return 0;
         }
         if ((address & 0x18000) == 0x18000)
         {
            vram_addr &= 0x17FFF;
         }
         value = READ32LE(vram + vram_addr);
         break;
      }
      case 7:
         value = READ32LE(oam + (address & 0x3FC));
         break;
      case 8:
      case 9:
      case 10:
      case 11:
      case 12:
         value = READ32LE(rom + (address & 0x1FFFFFC));
         break;
      case 13:
         if (cpuEEPROMEnabled)
         // no need to swap this
         {
            return eepromRead();
         }
         goto unreadable;
      case 14:
      case 15:
         if (cpuFlashEnabled | cpuSramEnabled)
         { // no need to swap this
            value = flashRead(address) * 0x01010101;
            break;
         }
      // default
      default:
      unreadable:
         if (cpuDmaHack)
         {
            value = cpuDmaLast;
         }
         else
         {
            value = CPUReadMemoryQuick(reg[15].I);
            if (!armState)
            {
               value &= 0xFFFF;
               value |= (value << 16);
            }
         }
   }
   int shift = (address & 3) << 3;
   return ROR(value, shift);
}

uint32_t CPUReadHalfWord(uint32_t address)
{
   uint32_t value = 0;
   switch (address >> 24)
   {
      case 0:
         if (address < 0x4000)
         {
            if (reg[15].I >> 24)
            {
               value = READ16LE(biosProtected + (address & 2));
            }
            else
            {
               value = READ16LE(bios + (address & 0x3FFE));
            }
         }
         else
         {
            goto unreadable;
         }
         break;
      case 2:
         value = READ16LE(workRAM + (address & 0x3FFFE));
         break;
      case 3:
         value = READ16LE(internalRAM + (address & 0x7FFE));
         break;
      case 4:
         if ((address < 0x4000400) && ioReadable[address & 0x3FE])
         {
            if ((address & 0xFF0) == 0x100)
            {
               switch (address & 0x10F)
               {
                  case 0x100:
                     if (timers.tm[0].On)
                     {
                        value = 0xFFFF - ((timers.tm[0].Ticks - cpuTotalTicks) >> timers.tm[0].ClockReload);
                     }
                     break;
                  case 0x104:
                     if (timers.tm[1].On && !(TM1CNT & 4))
                     {
                        value = 0xFFFF - ((timers.tm[1].Ticks - cpuTotalTicks) >> timers.tm[1].ClockReload);
                     }
                     break;
                  case 0x108:
                     if (timers.tm[2].On && !(TM2CNT & 4))
                     {
                        value = 0xFFFF - ((timers.tm[2].Ticks - cpuTotalTicks) >> timers.tm[2].ClockReload);
                     }
                     break;
                  case 0x10C:
                     if (timers.tm[3].On && !(TM3CNT & 4))
                     {
                        value = 0xFFFF - ((timers.tm[3].Ticks - cpuTotalTicks) >> timers.tm[3].ClockReload);
                     }
                     break;
               }
            }
            else
            {
               value = READ16LE(ioMem + (address & 0x3FE));
            }
         }
         else if ((address < 0x4000400) && ioReadable[address & 0x3FC])
         {
            return 0;
         }
         else
         {
            goto unreadable;
         }
         break;
      case 5:
         value = READ16LE(paletteRAM + (address & 0x3FE));
         break;
      case 6:
      {
         uint32_t vram_addr = (address & 0x1FFFE);
         if (((lcd.dispcnt & 7) > 2) && ((address & 0x1C000) == 0x18000))
         {
            value = 0;
            break;
         }
         if ((address & 0x18000) == 0x18000)
         {
            vram_addr &= 0x17FFF;
         }
         value = READ16LE(vram + (vram_addr));
         break;
      }
      case 7:
         value = READ16LE(oam + (address & 0x3FE));
         break;
      case 8:
      case 9:
      case 10:
      case 11:
      case 12:
         if (address == 0x80000c4 || address == 0x80000c6 || address == 0x80000c8)
         {
            value = rtcRead(address);
         }
         else
         {
            value = READ16LE(rom + (address & 0x1FFFFFE));
         }
         break;
      case 13:
         if (cpuEEPROMEnabled)
         // no need to swap this
         {
            return eepromRead();
         }
         goto unreadable;
      case 14:
      case 15:
         if (cpuFlashEnabled | cpuSramEnabled)
         {
            // no need to swap this
            value = flashRead(address) * 0x0101;
            break;
         }
      // default
      default:
      unreadable:
         if (cpuDmaHack)
         {
            value = cpuDmaLast & 0xFFFF;
         }
         else
         {
            uint32_t param = reg[15].I;
            if (armState)
            {
               param += (address & 2);
            }
            value = CPUReadHalfWordQuick(param);
         }
   }
   int shift = (address & 1) << 3;
   return ROR(value, shift);
}

int16_t CPUReadHalfWordSigned(uint32_t address)
{
   int32_t value = (int32_t)CPUReadHalfWord(address);
   if ((address & 1))
   {
      return (int8_t)value;
   }
   return (int16_t)value;
}

uint8_t CPUReadByte(uint32_t address)
{
   switch (address >> 24)
   {
      case 0:
         if (address < 0x4000)
         {
            if (reg[15].I >> 24)
            {
               return biosProtected[address & 3];
            }
            else
            {
               return bios[address & 0x3FFF];
            }
         }
         else
         {
            goto unreadable;
         }
      case 2:
         return workRAM[address & 0x3FFFF];
      case 3:
         return internalRAM[address & 0x7FFF];
      case 4:
         if ((address < 0x4000400) && ioReadable[address & 0x3FF])
         {
            return ioMem[address & 0x3FF];
         }
         else
         {
            goto unreadable;
         }
      case 5:
         return paletteRAM[address & 0x3FF];
      case 6:
         address = (address & 0x1FFFF);
         if (((lcd.dispcnt & 7) > 2) && ((address & 0x1C000) == 0x18000))
         {
            return 0;
         }
         if ((address & 0x18000) == 0x18000)
         {
            address &= 0x17FFF;
         }
         return vram[address];
      case 7:
         return oam[address & 0x3FF];
      case 8:
      case 9:
      case 10:
      case 11:
      case 12:
         return rom[address & 0x1FFFFFF];
      case 13:
         if (cpuEEPROMEnabled)
         {
            return eepromRead();
         }
         else
         {
            goto unreadable;
         }
      case 14:
      case 15:
         if (cpuSramEnabled | cpuFlashEnabled)
         {
            return flashRead(address);
         }

         switch (address & 0x00008f00)
         {
            case 0x8200:
               return systemGetSensorX() & 255;
            case 0x8300:
               return (systemGetSensorX() >> 8) | 0x80;
            case 0x8400:
               return systemGetSensorY() & 255;
            case 0x8500:
               return systemGetSensorY() >> 8;
         }
      // default
      default:
      unreadable:
         if (cpuDmaHack)
         {
            return cpuDmaLast & 0xFF;
         }
         else
         {
            uint32_t param = reg[15].I;
            if (armState)
            {
               param += (address & 3);
            }
            else
            {
               param += (address & 1);
            }
            return CPUReadByteQuick(param);
         }
   }
}

void CPUWriteMemory(uint32_t address, uint32_t value)
{
   switch (address >> 24)
   {
      case 0x02:
         WRITE32LE(workRAM + (address & 0x3FFFC), value);
         break;
      case 0x03:
         WRITE32LE(internalRAM + (address & 0x7FFC), value);
         break;
      case 0x04:
         if (address < 0x4000400)
         {
            CPUUpdateRegister((address & 0x3FC), (value & 0xFFFF));
            CPUUpdateRegister((address & 0x3FC) + 2, ((value >> 16) & 0xFFFF));
         }
         break;
      case 0x05:
         WRITE32LE(paletteRAM + (address & 0x3FC), value);
         break;
      case 0x06:
         address = (address & 0x1FFFC);
         if (((lcd.dispcnt & 7) > 2) && ((address & 0x1C000) == 0x18000))
         {
            return;
         }
         if ((address & 0x18000) == 0x18000)
         {
            address &= 0x17FFF;
         }
         WRITE32LE(vram + address, value);
         break;
      case 0x07:
         WRITE32LE(oam + (address & 0x3FC), value);
         break;
      case 0x0D:
         if (cpuEEPROMEnabled)
         {
            eepromWrite(value);
         }
         break;
      case 0x0E:
      case 0x0F:
         if ((!eepromInUse) | cpuSramEnabled | cpuFlashEnabled)
         {
            (*cpuSaveGameFunc)(address, (uint8_t)value);
         }
         break;
   }
}

void CPUWriteHalfWord(uint32_t address, uint16_t value)
{
   switch (address >> 24)
   {
      case 2:
         WRITE16LE(workRAM + (address & 0x3FFFE), value);
         break;
      case 3:
         WRITE16LE(internalRAM + (address & 0x7FFE), value);
         break;
      case 4:
         if (address < 0x4000400)
         {
            CPUUpdateRegister(address & 0x3FE, value);
         }
         break;
      case 5:
         WRITE16LE(paletteRAM + (address & 0x3FE), value);
         break;
      case 6:
         address = (address & 0x1FFFE);
         if (((lcd.dispcnt & 7) > 2) && ((address & 0x1C000) == 0x18000))
         {
            return;
         }
         if ((address & 0x18000) == 0x18000)
         {
            address &= 0x17FFF;
         }
         WRITE16LE(vram + address, value);
         break;
      case 7:
         WRITE16LE(oam + (address & 0x3FE), value);
         break;
      case 8:
      case 9:
         switch (address) {
            case 0x80000c4:
            case 0x80000c6:
            case 0x80000c8:
               (void)rtcWrite(address, value);
               break;
         }
         break;
      case 13:
         if (cpuEEPROMEnabled)
         {
            eepromWrite((uint8_t)value);
         }
         break;
      case 14:
      case 15:
         if ((!eepromInUse) | cpuSramEnabled | cpuFlashEnabled)
         {
            (*cpuSaveGameFunc)(address, (uint8_t)value);
         }
         break;
   }
}

void CPUWriteByte(uint32_t address, uint8_t value)
{
   switch (address >> 24)
   {
      case 2:
         workRAM[address & 0x3FFFF] = value;
         break;
      case 3:
         internalRAM[address & 0x7FFF] = value;
         break;
      case 4:
         if (address < 0x4000400)
         {
            switch (address & 0x3FF)
            {
               case 0x60:
               case 0x61:
               case 0x62:
               case 0x63:
               case 0x64:
               case 0x65:
               case 0x68:
               case 0x69:
               case 0x6c:
               case 0x6d:
               case 0x70:
               case 0x71:
               case 0x72:
               case 0x73:
               case 0x74:
               case 0x75:
               case 0x78:
               case 0x79:
               case 0x7c:
               case 0x7d:
               case 0x80:
               case 0x81:
               case 0x84:
               case 0x85:
               case 0x90:
               case 0x91:
               case 0x92:
               case 0x93:
               case 0x94:
               case 0x95:
               case 0x96:
               case 0x97:
               case 0x98:
               case 0x99:
               case 0x9a:
               case 0x9b:
               case 0x9c:
               case 0x9d:
               case 0x9e:
               case 0x9f:
                  soundEvent8(address & 0xFF, value);
                  break;
               case 0x301: // HALTCNT, undocumented
                  if (value == 0x80)
                  {
                     stopState = true;
                  }
                  holdState = 1;
                  holdType = -1;
                  cpuNextEvent = cpuTotalTicks;
                  break;
               default: // every other register
                  uint32_t lowerBits = address & 0x3FE;
                  uint16_t tmp = READ16LE(ioMem + lowerBits);
                  if (address & 1)
                  {
                     tmp = (tmp & 0x00FF) | (value << 8);
                  }
                  else
                  {
                     tmp = (tmp & 0xFF00) | value;
                  }
                  CPUUpdateRegister(lowerBits, tmp);
                  break;
            }
            break;
         }
         break;
      case 5:
         // no need to switch
         WRITE16LE(paletteRAM + (address & 0x3FE), (value << 8) | value);
         break;
      case 6:
         address = (address & 0x1FFFE);
         if (((lcd.dispcnt & 7) > 2) && ((address & 0x1C000) == 0x18000))
         {
            return;
         }
         if ((address & 0x18000) == 0x18000)
         {
            address &= 0x17FFF;
         }
         // no need to switch
         // byte writes to OBJ VRAM are ignored
         if ((address) < objTilesAddress[((lcd.dispcnt & 7) + 1) >> 2])
         {
            WRITE16LE(vram + address, (value << 8) | value);
         }
         break;
      case 7:
         // no need to switch
         // byte writes to OAM are ignored
         //    *((uint16_t *)&oam[address & 0x3FE]) = (value << 8) | value;
         break;
      case 13:
         if (cpuEEPROMEnabled)
         {
            eepromWrite(value);
         }
         break;
      case 14:
      case 15:
         if ((saveType != 5) && ((!eepromInUse) | cpuSramEnabled | cpuFlashEnabled))
         {
            (*cpuSaveGameFunc)(address, value);
         }
         break;
   }
}

void GBAMemoryCleanup(void)
{
   if (rom != NULL)
   {
      free(rom);
      rom = NULL;
   }

   if (vram != NULL)
   {
      free(vram);
      vram = NULL;
   }

   if (paletteRAM != NULL)
   {
      free(paletteRAM);
      paletteRAM = NULL;
   }

   if (internalRAM != NULL)
   {
      free(internalRAM);
      internalRAM = NULL;
   }

   if (workRAM != NULL)
   {
      free(workRAM);
      workRAM = NULL;
   }

   if (bios != NULL)
   {
      free(bios);
      bios = NULL;
   }

   if (pix != NULL)
   {
      free(pix);
      pix = NULL;
   }

   if (oam != NULL)
   {
      free(oam);
      oam = NULL;
   }

   if (ioMem != NULL)
   {
      free(ioMem);
      ioMem = NULL;
   }
}

bool GBAMemoryInit(void)
{
   if (rom != NULL)
      CPUCleanUp();

   bios = (uint8_t*)malloc(SIZE_BIOS);
   workRAM = (uint8_t*)malloc(SIZE_WRAM);
   internalRAM = (uint8_t*)malloc(SIZE_IRAM);
   ioMem = (uint8_t*)malloc(SIZE_IOMEM);
   paletteRAM = (uint8_t*)malloc(SIZE_PRAM);
   vram = (uint8_t*)malloc(SIZE_VRAM);
   oam = (uint8_t*)malloc(SIZE_OAM);
   rom = (uint8_t*)malloc(SIZE_ROM);
   pix = (pixFormat*)malloc(GBA_WIDTH * GBA_HEIGHT * 4);

   memset(bios, 1, SIZE_BIOS);
   memset(workRAM, 1, SIZE_WRAM);
   memset(internalRAM, 1, SIZE_IRAM);
   memset(ioMem, 1, SIZE_IOMEM);
   memset(paletteRAM, 1, SIZE_PRAM);
   memset(vram, 1, SIZE_VRAM);
   memset(oam, 1, SIZE_OAM);
   memset(rom, 0, SIZE_ROM);
   memset(pix, 1, GBA_WIDTH * GBA_HEIGHT * 4);

   if (rom == NULL || workRAM == NULL || bios == NULL || internalRAM == NULL ||
      paletteRAM == NULL || vram == NULL || oam == NULL || pix == NULL || ioMem == NULL)
   {
      CPUCleanUp();
      return false;
   }

   flashInit();
   eepromInit();
   CPUUpdateRenderBuffers(true);

   return true;
}
