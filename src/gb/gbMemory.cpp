#include <stdint.h>

#include "gbMemory.h"
#include "ConfigManager.h"
#include "GBALink.h"
#include "gbGlobals.h"
#include "gbMBC.h"
#include "gbSGB.h"
#include "gbSound.h"
#include "gb.h"
#include "System.h"
#include "gbCheats.h"

extern uint8_t register_IF;
extern uint8_t register_STAT;
extern uint8_t register_IE;

extern int gbSerialOn;
extern int gbSerialTicks;
extern int gbSerialBits;
extern int gbDivTicks;
extern int gbTimerOn;
extern int gbInternalTimer;
extern int gbTimerClockTicks;
extern bool gbTimerModeChange;
extern bool gbTimerOnChange;
extern int gbTimerMode;
extern int gbTimerTicks;
extern int clockTicks;
extern int register_LCDCBusy;
extern int gbLcdTicks;
extern int gbLcdTicksDelayed;
extern int gbLcdLYIncrementTicks;
extern int gbLcdLYIncrementTicksDelayed;
extern int gbLcdMode;
extern int gbLcdModeDelayed;
extern bool gbLYChangeHappened;
extern int gbInt48Signal;
extern int gbWhiteScreen;
extern int gbScreenTicks;
extern bool gbLCDChangeHappened;
extern bool inBios;
extern int gbVramBank;
extern int gbWramBank;
extern int gbHdmaSource;
extern int gbHdmaDestination;
extern int gbHdmaBytes;
extern int gbHdmaOn;

extern int GBSERIAL_CLOCK_TICKS;
extern int GBDIV_CLOCK_TICKS;
extern int GBTIMER_MODE_0_CLOCK_TICKS;
extern int GBTIMER_MODE_1_CLOCK_TICKS;
extern int GBTIMER_MODE_2_CLOCK_TICKS;
extern int GBTIMER_MODE_3_CLOCK_TICKS;
extern int GBLCD_MODE_2_CLOCK_TICKS;
extern int GBLY_INCREMENT_CLOCK_TICKS;
extern int GBLCD_MODE_0_CLOCK_TICKS;
extern int GBLCD_MODE_2_CLOCK_TICKS;
extern int GBLCD_MODE_3_CLOCK_TICKS;

extern uint16_t gbJoymask[4];

static INLINE bool gbCgbPaletteAccessValid(void)
{
   // No access to gbPalette during mode 3 (Color Panel Demo)
   if (allowColorizerHack() || ((gbLcdModeDelayed != 3) && (!((gbLcdMode == 0) && (gbLcdTicks >= (GBLCD_MODE_0_CLOCK_TICKS - gbSpritesTicks[299] - 1)))) && (!gbSpeed)) || (gbSpeed && ((gbLcdMode == 1) || (gbLcdMode == 2) || ((gbLcdMode == 3) && (gbLcdTicks > (GBLCD_MODE_3_CLOCK_TICKS - 2))) || ((gbLcdMode == 0) && (gbLcdTicks <= (GBLCD_MODE_0_CLOCK_TICKS - gbSpritesTicks[299] - 2))))))
      return true;
   return false;
}

static INLINE bool gbVramReadAccessValid(void)
{
   // A lot of 'ugly' checks... But only way to emulate this particular behaviour...
   if (allowColorizerHack() || ((gbHardware & 0xa) && ((gbLcdModeDelayed != 3) || (((register_LY == 0) && (gbScreenOn == false) && (register_LCDC & 0x80)) && (gbLcdLYIncrementTicksDelayed == (GBLY_INCREMENT_CLOCK_TICKS - GBLCD_MODE_2_CLOCK_TICKS))))) || ((gbHardware & 0x5) && (gbLcdModeDelayed != 3) && ((gbLcdMode != 3) || ((register_LY == 0) && ((gbScreenOn == false) && (register_LCDC & 0x80)) && (gbLcdLYIncrementTicks == (GBLY_INCREMENT_CLOCK_TICKS - GBLCD_MODE_2_CLOCK_TICKS))))))
      return true;
   return false;
}

static INLINE bool gbVramWriteAccessValid(void)
{
   if (allowColorizerHack() ||
       // No access to Vram during mode 3
       // (used to emulate the gfx differences between GB & GBC-GBA/SP in Stunt Racer)
       (gbLcdModeDelayed != 3) ||
       // This part is used to emulate a small difference between hardwares
       // (check 8-in-1's arrow on GBA/GBC to verify it)
       ((register_LY == 0) && ((gbHardware & 0xa) && (gbScreenOn == false) && (register_LCDC & 0x80)) && (gbLcdLYIncrementTicksDelayed == (GBLY_INCREMENT_CLOCK_TICKS - GBLCD_MODE_2_CLOCK_TICKS))))
      return true;
   return false;
}

static void gbWriteIO(uint16_t address, uint8_t value)
{
   switch (address & 0x00ff)
   {
   case 0x00:
   {
      gbMemory[REG_P1] = ((gbMemory[REG_P1] & 0xcf) | (value & 0x30) | 0xc0);
      if (gbSgbMode)
      {
         gbSgbDoBitTransfer(value);
      }
      return;
   }

   case 0x01:
   {
      gbMemory[REG_SB] = value;
      return;
   }

   // serial control
   case 0x02:
   {
      gbSerialOn = (value & 0x80);
#ifndef NO_LINK
      //trying to detect whether the game has exited multiplay mode, pokemon blue start w/ 0x7e while pocket racing start w/ 0x7c
      if (EmuReseted || (gbMemory[REG_SC] & 0x7c) || (value & 0x7c) || (!(value & 0x81)))
      {
         LinkFirstTime = true;
      }
      EmuReseted = false;
      gbMemory[REG_SC] = value;
      if (gbSerialOn && (GetLinkMode() == LINK_GAMEBOY_IPC || GetLinkMode() == LINK_GAMEBOY_SOCKET || GetLinkMode() == LINK_DISCONNECTED || winGbPrinterEnabled))
      {

         gbSerialTicks = GBSERIAL_CLOCK_TICKS;

         LinkIsWaiting = true;

         //Do data exchange, master initiate the transfer
         //may cause visual artifact if not processed immediately, is it due to IRQ stuff or invalid data being exchanged?
         //internal clock
         if ((value & 1))
         {
            if (gbSerialFunction)
            {
               gbSIO_SC = value;
               gbMemory[REG_SB] = gbSerialFunction(gbMemory[REG_SB]); //gbSerialFunction/gbStartLink/gbPrinter
            }
            else
               gbMemory[REG_SB] = 0xff;
            gbMemory[REG_SC] &= 0x7f;
            gbSerialOn = 0;
            gbMemory[IF_FLAG] = register_IF |= 8;
         }
#ifdef OLD_GB_LINK
         if (linkConnected)
         {
            if (value & 1)
            {
               linkSendByte(0x100 | gbMemory[0xFF01]);
               Sleep(5);
            }
         }
#endif
      }

      gbSerialBits = 0;
#else // NO_LINK
      if ((value & 1))
      {
         gbMemory[REG_SB] = 0xff;
         gbMemory[REG_SC] &= 0x7f;
         gbSerialOn = 0;
         gbMemory[IF_FLAG] = register_IF |= 8;
      }
#endif // NO_LINK
      return;
   }

   case 0x04:
   {
      // DIV register resets on any write
      // (not totally perfect, but better than nothing)
      gbMemory[REG_DIV] = 0;
      gbDivTicks = GBDIV_CLOCK_TICKS;
      // Another weird timer 'bug' :
      // Writing to DIV register resets the internal timer,
      // and can also increase TIMA/trigger an interrupt
      // in some cases...
      if (gbTimerOn && !(gbInternalTimer & (gbTimerClockTicks >> 1)))
      {
         gbMemory[REG_TIMA]++;
         // timer overflow!
         if (gbMemory[REG_TIMA] == 0)
         {
            // reload timer modulo
            gbMemory[REG_TIMA] = gbMemory[REG_TMA];

            // flag interrupt
            gbMemory[IF_FLAG] = register_IF |= 4;
         }
      }
      gbInternalTimer = 0xff;
      return;
   }
   case 0x05:
      gbMemory[REG_TIMA] = value;
      return;

   case 0x06:
      gbMemory[REG_TMA] = value;
      return;

   // TIMER control
   case 0x07:
   {

      gbTimerModeChange = (((value & 3) != (gbMemory[REG_TAC] & 3)) && (value & gbMemory[REG_TAC] & 4)) ? true : false;
      gbTimerOnChange = (((value ^ gbMemory[REG_TAC]) & 4) == 4) ? true : false;

      gbTimerOn = (value & 4);

      if (gbTimerOnChange || gbTimerModeChange)
      {
         gbTimerMode = value & 3;

         switch (gbTimerMode)
         {
         case 0:
            gbTimerClockTicks = GBTIMER_MODE_0_CLOCK_TICKS;
            break;
         case 1:
            gbTimerClockTicks = GBTIMER_MODE_1_CLOCK_TICKS;
            break;
         case 2:
            gbTimerClockTicks = GBTIMER_MODE_2_CLOCK_TICKS;
            break;
         case 3:
            gbTimerClockTicks = GBTIMER_MODE_3_CLOCK_TICKS;
            break;
         }
      }

      // This looks weird, but this emulates a bug in which register TIMA
      // is increased when writing to register TAC
      // (This fixes Korodice's long-delay between menus bug).

      if (gbTimerOnChange || gbTimerModeChange)
      {
         bool temp = false;

         if ((gbTimerOn && !gbTimerModeChange) && (gbTimerMode & 2) && !(gbInternalTimer & 0x80) && (gbInternalTimer & (gbTimerClockTicks >> 1)) && !(gbInternalTimer & (gbTimerClockTicks >> 5)))
            temp = true;
         else if ((!gbTimerOn && !gbTimerModeChange && gbTimerOnChange) && ((gbTimerTicks - 1) < (gbTimerClockTicks >> 1)))
            temp = true;
         else if (gbTimerOn && gbTimerModeChange && !gbTimerOnChange)
         {
            switch (gbTimerMode & 3)
            {
            case 0x00:
               temp = false;
               break;
            case 0x01:
               if (((gbInternalTimer & 0x82) == 2) && (gbTimerTicks > (clockTicks + 1)))
                  temp = true;
               break;
            case 0x02:
               if (((gbInternalTimer & 0x88) == 0x8) && (gbTimerTicks > (clockTicks + 1)))
                  temp = true;
               break;
            case 0x03:
               if (((gbInternalTimer & 0xA0) == 0x20) && (gbTimerTicks > (clockTicks + 1)))
                  temp = true;
               break;
            }
         }

         if (temp)
         {
            gbMemory[REG_TIMA]++;
            if ((gbMemory[REG_TIMA] & 0xff) == 0)
            {
               // timer overflow!

               // reload timer modulo
               gbMemory[REG_TIMA] = gbMemory[REG_TMA];

               // flag interrupt
               gbMemory[IF_FLAG] = register_IF |= 4;
            }
         }
      }
      gbMemory[REG_TAC] = value;
      return;
   }

   case 0x0f:
   {
      gbMemory[IF_FLAG] = register_IF = value;
      return;
   }

   case 0x10:
   case 0x11:
   case 0x12:
   case 0x13:
   case 0x14:
   case 0x15:
   case 0x16:
   case 0x17:
   case 0x18:
   case 0x19:
   case 0x1a:
   case 0x1b:
   case 0x1c:
   case 0x1d:
   case 0x1e:
   case 0x1f:
   case 0x20:
   case 0x21:
   case 0x22:
   case 0x23:
   case 0x24:
   case 0x25:
   case 0x26:
   case 0x27:
   case 0x28:
   case 0x29:
   case 0x2a:
   case 0x2b:
   case 0x2c:
   case 0x2d:
   case 0x2e:
   case 0x2f:
   case 0x30:
   case 0x31:
   case 0x32:
   case 0x33:
   case 0x34:
   case 0x35:
   case 0x36:
   case 0x37:
   case 0x38:
   case 0x39:
   case 0x3a:
   case 0x3b:
   case 0x3c:
   case 0x3d:
   case 0x3e:
   case 0x3f:
      gbSoundEvent(address, value);
      return;

   case 0x40:
   {
      int lcdChange = (register_LCDC & 0x80) ^ (value & 0x80);

      // don't draw the window if it was not enabled and not being drawn before
      if (!(register_LCDC & 0x20) && (value & 0x20) && gbWindowLine == -1 && register_LY > gbMemory[REG_WY])
         gbWindowLine = 146;
      // 007 fix : don't draw the first window's 1st line if it's enable 'too late'
      // (ie. if register LY == register WY when enabling it)
      // and move it to the next line
      else if (!(register_LCDC & 0x20) && (value & 0x20) && (register_LY == gbMemory[REG_WY]))
         gbWindowLine = -2;

      gbMemory[REG_LCDC] = register_LCDC = value;

      if (lcdChange)
      {
         if ((value & 0x80) && (!register_LCDCBusy))
         {
            //  if (!gbWhiteScreen && !gbSgbMask)
            //    systemDrawScreen();

            gbLcdTicks = GBLCD_MODE_2_CLOCK_TICKS - (gbSpeed ? 2 : 1);
            gbLcdTicksDelayed = GBLCD_MODE_2_CLOCK_TICKS - (gbSpeed ? 1 : 0);
            gbLcdLYIncrementTicks = GBLY_INCREMENT_CLOCK_TICKS - (gbSpeed ? 2 : 1);
            gbLcdLYIncrementTicksDelayed = GBLY_INCREMENT_CLOCK_TICKS - (gbSpeed ? 1 : 0);
            gbLcdMode = 2;
            gbLcdModeDelayed = 2;
            gbMemory[REG_STAT] = register_STAT = (register_STAT & 0xfc) | 2;
            gbMemory[REG_LY] = register_LY = 0x00;
            gbInt48Signal = 0;
            gbLYChangeHappened = false;
            gbLCDChangeHappened = false;
            gbWindowLine = 146;
            oldRegister_WY = 146;

            // Fix for Namco Gallery Vol.2
            // (along with updating register_LCDC at the start of 'case 0x40') :
            if (register_STAT & 0x20)
            {
               // send LCD interrupt only if no interrupt 48h signal...
               if (!gbInt48Signal)
               {
                  gbMemory[IF_FLAG] = register_IF |= 2;
               }
               gbInt48Signal |= 4;
            }
            gbCompareLYToLYC();
         }
         else
         {
            register_LCDCBusy = clockTicks + 6;

            //used to update the screen with white lines when it's off.
            //(it looks strange, but it's pretty accurate)

            gbWhiteScreen = 0;

            gbScreenTicks = ((150 - register_LY) * GBLY_INCREMENT_CLOCK_TICKS + (49 << (gbSpeed ? 1 : 0)));

            // disable the screen rendering
            gbScreenOn = false;
            gbLcdTicks = 0;
            gbLcdMode = 0;
            gbLcdModeDelayed = 0;
            gbMemory[REG_STAT] = register_STAT &= 0xfc;
            gbInt48Signal = 0;
         }
      }
      return;
   }

   // STAT
   case 0x41:
   {
      //register_STAT = (register_STAT & 0x87) |
      //      (value & 0x7c);
      gbMemory[REG_STAT] = register_STAT = (value & 0xf8) | (register_STAT & 0x07); // fix ?
      // TODO:
      // GB bug from Devrs FAQ
      // http://www.devrs.com/gb/files/faqs.html#GBBugs
      // 2018-7-26 Backported STAT register bug behavior
      // Corrected : it happens if Lcd Mode < 2, but also if LY == LYC whatever
      // Lcd Mode is, and if !gbInt48Signal in all cases. The screen being off
      // doesn't matter (the bug will still happen).
      // That fixes 'Satoru Nakajima - F-1 Hero' crash bug.
      // Games below relies on this bug, , and are incompatible with the GBC.
      // - Road Rash: crash after player screen
      // - Zerg no Densetsu: crash right after showing a small portion of intro
      // - 2019-07-18 - Speedy Gonzalez status bar relies on this as well.

      if (((gbHardware & 5) || ((gbHardware & 2) && gbMemory[REG_UNK6C] & 1))
          && (((!gbInt48Signal) && (gbLcdMode < 2) && (register_LCDC & 0x80))
              || (register_LY == gbMemory[REG_LYC])))
      {
         // send LCD interrupt only if no interrupt 48h signal...
         if (!gbInt48Signal)
            gbMemory[IF_FLAG] = register_IF |= 2;
      }

      gbInt48Signal &= ((register_STAT >> 3) & 0xF);

      if ((register_LCDC & 0x80))
      {
         if ((register_STAT & 0x08) && (gbLcdMode == 0))
         {
            if (!gbInt48Signal)
            {
               gbMemory[IF_FLAG] = register_IF |= 2;
            }
            gbInt48Signal |= 1;
         }
         if ((register_STAT & 0x10) && (gbLcdMode == 1))
         {
            if (!gbInt48Signal)
            {
               gbMemory[IF_FLAG] = register_IF |= 2;
            }
            gbInt48Signal |= 2;
         }
         if ((register_STAT & 0x20) && (gbLcdMode == 2))
         {
            if (!gbInt48Signal)
            {
               gbMemory[IF_FLAG] = register_IF |= 2;
            }
            //gbInt48Signal |= 4;
         }
         gbCompareLYToLYC();

         gbMemory[IF_FLAG] = register_IF;
         gbMemory[REG_STAT] = register_STAT;
      }
      return;
   }

   // SCY
   case 0x42:
   {
      int temp = -1;

      if ((gbLcdMode == 3) || (gbLcdModeDelayed == 3))
         temp = ((GBLY_INCREMENT_CLOCK_TICKS - GBLCD_MODE_2_CLOCK_TICKS) - gbLcdLYIncrementTicks);

      if (temp >= 0)
      {
         for (int i = temp << (gbSpeed ? 1 : 2); i < 300; i++)
            if (temp < 300)
               gbSCYLine[i] = value;
      }

      else
         memset(gbSCYLine, value, sizeof(gbSCYLine));

      gbMemory[REG_SCY] = value;
      return;
   }

   // SCX
   case 0x43:
   {
      int temp = -1;

      if (gbLcdModeDelayed == 3)
         temp = ((GBLY_INCREMENT_CLOCK_TICKS - GBLCD_MODE_2_CLOCK_TICKS) - gbLcdLYIncrementTicksDelayed);

      if (temp >= 0)
      {
         for (int i = temp << (gbSpeed ? 1 : 2); i < 300; i++)
            if (temp < 300)
               gbSCXLine[i] = value;
      }

      else
         memset(gbSCXLine, value, sizeof(gbSCXLine));

      gbMemory[REG_SCX] = value;
      return;
   }

   // LY
   case 0x44:
   {
      // read only
      return;
   }

   // LYC
   case 0x45:
   {
      if (gbMemory[REG_LYC] != value)
      {
         gbMemory[REG_LYC] = value;
         if (register_LCDC & 0x80)
         {
            gbCompareLYToLYC();
         }
      }
      return;
   }

   // DMA!
   case 0x46:
   {
      int source = value * 0x0100;

      gbCopyMemory(0xfe00,
          source,
          0xa0);
      gbMemory[REG_DMA] = value;
      return;
   }

   // BGP
   case 0x47:
   {

      int temp = -1;

      gbMemory[REG_BGP] = value;

      if (gbLcdModeDelayed == 3)
         temp = ((GBLY_INCREMENT_CLOCK_TICKS - GBLCD_MODE_2_CLOCK_TICKS) - gbLcdLYIncrementTicksDelayed);

      if (temp >= 0)
      {
         for (int i = temp << (gbSpeed ? 1 : 2); i < 300; i++)
            if (temp < 300)
               gbBgpLine[i] = value;
      }
      else
         memset(gbBgpLine, value, sizeof(gbBgpLine));

      gbBgp[0] = value & 0x03;
      gbBgp[1] = (value & 0x0c) >> 2;
      gbBgp[2] = (value & 0x30) >> 4;
      gbBgp[3] = (value & 0xc0) >> 6;
      break;
   }

   // OBP0
   case 0x48:
   {
      int temp = -1;

      gbMemory[REG_OBP0] = value;

      if (gbLcdModeDelayed == 3)
         temp = ((GBLY_INCREMENT_CLOCK_TICKS - GBLCD_MODE_2_CLOCK_TICKS) - gbLcdLYIncrementTicksDelayed);

      if (temp >= 0)
      {
         for (int i = temp << (gbSpeed ? 1 : 2); i < 300; i++)
            if (temp < 300)
               gbObp0Line[i] = value;
      }
      else
         memset(gbObp0Line, value, sizeof(gbObp0Line));

      gbObp0[0] = value & 0x03;
      gbObp0[1] = (value & 0x0c) >> 2;
      gbObp0[2] = (value & 0x30) >> 4;
      gbObp0[3] = (value & 0xc0) >> 6;
      break;
   }

   // OBP1
   case 0x49:
   {
      int temp = -1;

      gbMemory[REG_OBP1] = value;

      if (gbLcdModeDelayed == 3)
         temp = ((GBLY_INCREMENT_CLOCK_TICKS - GBLCD_MODE_2_CLOCK_TICKS) - gbLcdLYIncrementTicksDelayed);

      if (temp >= 0)
      {
         for (int i = temp << (gbSpeed ? 1 : 2); i < 300; i++)
            if (temp < 300)
               gbObp1Line[i] = value;
      }
      else
         memset(gbObp1Line, value, sizeof(gbObp1Line));

      gbObp1[0] = value & 0x03;
      gbObp1[1] = (value & 0x0c) >> 2;
      gbObp1[2] = (value & 0x30) >> 4;
      gbObp1[3] = (value & 0xc0) >> 6;
      break;
   }

   // WY
   case 0x4a:
      gbMemory[REG_WY] = value;
      if ((register_LY <= gbMemory[REG_WY]) && ((gbWindowLine < 0) || (gbWindowLine >= 144)))
      {
         gbWindowLine = -1;
         oldRegister_WY = gbMemory[REG_WY];
      }
      return;

   // WX
   case 0x4b:
      gbMemory[REG_WX] = value;
      return;

   // BOOTROM disable register (also gbCgbMode enabler if value & 0x10 ?)
   case 0x50:
   {
      if (inBios && (value & 1))
      {
         gbMemoryMap[GB_MEM_CART_BANK0] = &gbRom[0x0000];
         if (gbHardware & 5)
         {
            memcpy((uint8_t *)(gbRom + 0x100), (uint8_t *)(gbMemory + 0x100), 0xF00);
         }
         inBios = false;
      }
   }
   break;

   case 0x6c:
   {
      gbMemory[REG_UNK6C] = 0xfe | value;
      return;
   }

   case 0x75:
   {
      gbMemory[REG_UNK75] = 0x8f | value;
      return;
   }

   case 0xff:
   {
      gbMemory[0xffff] = register_IE = value;
      return;
   }

   default:
   {
      if (gbCgbMode)
      {
         switch (address & 0x00ff)
         {
         // KEY1
         case 0x4d:
            gbMemory[REG_KEY1] = (gbMemory[REG_KEY1] & 0x80) | (value & 1) | 0x7e;
            return;

         // VBK
         case 0x4f:
         {
            value &= 1;
            if (value == gbVramBank)
               return;
            int vramAddress = value << 13;
            gbMemoryMap[GB_MEM_VRAM] = &gbVram[vramAddress];
            gbMemoryMap[GB_MEM_VRAM + 1] = &gbVram[vramAddress + 0x1000];
            gbVramBank = value;
            gbMemory[REG_VBK] = value;
            return;
         }

         // HDMA1
         case 0x51:
            if (value > 0x7f && value < 0xa0)
               value = 0;
            gbHdmaSource = (value << 8) | (gbHdmaSource & 0xf0);
            gbMemory[REG_HDMA1] = value;
            return;

         // HDMA2
         case 0x52:
            value &= 0xf0;
            gbHdmaSource = (gbHdmaSource & 0xff00) | (value);
            gbMemory[REG_HDMA2] = value;
            return;

         // HDMA3
         case 0x53:
            value &= 0x1f;
            gbHdmaDestination = (value << 8) | (gbHdmaDestination & 0xf0);
            gbHdmaDestination |= 0x8000;
            gbMemory[REG_HDMA3] = value;
            return;

         // HDMA4
         case 0x54:
            value &= 0xf0;
            gbHdmaDestination = (gbHdmaDestination & 0x1f00) | value;
            gbHdmaDestination |= 0x8000;
            gbMemory[REG_HDMA4] = value;
            return;

         // HDMA5
         case 0x55:
            gbHdmaBytes = 16 + (value & 0x7f) * 16;
            if (gbHdmaOn)
            {
               if (value & 0x80)
               {
                  gbMemory[REG_HDMA5] = (value & 0x7f);
               }
               else
               {
                  gbMemory[REG_HDMA5] = 0xff;
                  gbHdmaOn = 0;
               }
            }
            else
            {
               if (value & 0x80)
               {
                  gbHdmaOn = 1;
                  gbMemory[REG_HDMA5] = value & 0x7f;
                  if (gbLcdModeDelayed == 0)
                     gbDoHdma();
               }
               else
               {
                  // we need to take the time it takes to complete the transfer into
                  // account... according to GB DEV FAQs, the setup time is the same
                  // for single and double speed, but the actual transfer takes the
                  // same time
                  if (gbSpeed)
                     gbDmaTicks = 2 + 16 * ((value & 0x7f) + 1);
                  else
                     gbDmaTicks = 1 + 8 * ((value & 0x7f) + 1);

                  gbCopyMemory((gbHdmaDestination & 0x1ff0) | 0x8000,
                      gbHdmaSource & 0xfff0,
                      gbHdmaBytes);
                  gbHdmaDestination += gbHdmaBytes;
                  gbHdmaSource += gbHdmaBytes;

                  gbMemory[REG_HDMA1] = 0xff; // = (gbHdmaSource >> 8) & 0xff;
                  gbMemory[REG_HDMA2] = 0xff; // = gbHdmaSource & 0xf0;
                  gbMemory[REG_HDMA3] = 0xff; // = ((gbHdmaDestination - 0x8000) >> 8) & 0x1f;
                  gbMemory[REG_HDMA4] = 0xff; // = gbHdmaDestination & 0xf0;
                  gbMemory[REG_HDMA5] = 0xff;
               }
            }
            return;

         // BCPS
         case 0x68:
         {
            int paletteIndex = (value & 0x3f) >> 1;
            int paletteHiLo = (value & 0x01);
            gbMemory[REG_BCPS] = value;
            gbMemory[REG_BCPD] = (paletteHiLo ? (gbPalette[paletteIndex] >> 8) : (gbPalette[paletteIndex] & 0x00ff));
            return;
         }

         // BCPD
         case 0x69:
         {
            int v = gbMemory[REG_BCPS];
            int paletteIndex = (v & 0x3f) >> 1;
            int paletteHiLo = (v & 0x01);
            if (gbCgbPaletteAccessValid())
            {
               gbMemory[REG_BCPD] = value;
               gbPalette[paletteIndex] = (paletteHiLo ? ((value << 8) | (gbPalette[paletteIndex] & 0xff)) : ((gbPalette[paletteIndex] & 0xff00) | (value))) & 0x7fff;
            }
            if (gbMemory[REG_BCPS] & 0x80)
            {
               int index = ((gbMemory[REG_BCPS] & 0x3f) + 1) & 0x3f;
               gbMemory[REG_BCPS] = (gbMemory[REG_BCPS] & 0x80) | index;
               gbMemory[REG_BCPD] = (index & 1 ? (gbPalette[index >> 1] >> 8) : (gbPalette[index >> 1] & 0x00ff));
            }
            return;
         }

         // OCPS
         case 0x6a:
         {
            int paletteIndex = (value & 0x3f) >> 1;
            int paletteHiLo = (value & 0x01);
            paletteIndex += 32;
            gbMemory[REG_OCPS] = value;
            gbMemory[REG_OCPD] = (paletteHiLo ? (gbPalette[paletteIndex] >> 8) : (gbPalette[paletteIndex] & 0x00ff));
            return;
         }

         // OCPD
         case 0x6b:
         {
            int v = gbMemory[REG_OCPS];
            int paletteIndex = (v & 0x3f) >> 1;
            int paletteHiLo = (v & 0x01);
            paletteIndex += 32;
            if (gbCgbPaletteAccessValid())
            {
               gbMemory[REG_OCPD] = value;
               gbPalette[paletteIndex] = (paletteHiLo ? ((value << 8) | (gbPalette[paletteIndex] & 0xff)) : ((gbPalette[paletteIndex] & 0xff00) | (value))) & 0x7fff;
            }
            if (gbMemory[REG_OCPS] & 0x80)
            {
               int index = ((gbMemory[REG_OCPS] & 0x3f) + 1) & 0x3f;
               gbMemory[REG_OCPS] = (gbMemory[REG_OCPS] & 0x80) | index;
               gbMemory[REG_OCPD] = (index & 1 ? (gbPalette[(index >> 1) + 32] >> 8) : (gbPalette[(index >> 1) + 32] & 0x00ff));
            }
            return;
         }

         // SVBK
         case 0x70:
         {
            value &= 7;
            int bank = value;
            if (value == 0)
               bank = 1;
            if (bank == gbWramBank)
               return;
            int wramAddress = bank << 12;
            gbMemoryMap[GB_MEM_WRAM_BANK1] = &gbWram[wramAddress];
            gbWramBank = bank;
            gbMemory[REG_SVBK] = value;
            return;
         }

         default:
            break;
         }
      }
      break;
   }
   }

   gbMemory[address] = value;
}

static uint8_t gbReadIO(uint16_t address)
{
   switch (address & 0x00ff)
   {
   case 0x00:
   {
      int P1 = gbMemory[REG_P1];
      if (gbSgbMode)
      {
         gbSgbReadingController |= 4;
         gbSgbResetPacketState();
      }
      if ((P1 & 0x30) == 0x20)
      {
         int joy = 0;
         P1 &= 0xf0;
         if (gbSgbMode && gbSgbMultiplayer)
         {
            switch (gbSgbNextController)
            {
            case 0x0f: joy = 0; break;
            case 0x0e: joy = 1; break;
            case 0x0d: joy = 2; break;
            case 0x0c: joy = 3; break;
            default: joy = 0; break;
            }
         }
         uint16_t joystate = gbJoymask[joy];
         if (!(joystate & 0x80))
            P1 |= 0x08;
         if (!(joystate & 0x40))
            P1 |= 0x04;
         if (!(joystate & 0x20))
            P1 |= 0x02;
         if (!(joystate & 0x10))
            P1 |= 0x01;
         gbMemory[REG_P1] = P1;
      }
      else if ((P1 & 0x30) == 0x10)
      {
         uint16_t joy = 0;
         P1 &= 0xf0;
         if (gbSgbMode && gbSgbMultiplayer)
         {
            switch (gbSgbNextController)
            {
            case 0x0f: joy = 0; break;
            case 0x0e: joy = 1; break;
            case 0x0d: joy = 2; break;
            case 0x0c: joy = 3; break;
            default: joy = 0; break;
            }
         }
         uint16_t joystate = gbJoymask[joy];
         if (!(joystate & 0x08))
            P1 |= 0x08;
         if (!(joystate & 0x04))
            P1 |= 0x04;
         if (!(joystate & 0x02))
            P1 |= 0x02;
         if (!(joystate & 0x01))
            P1 |= 0x01;
         gbMemory[REG_P1] = P1;
      }
      else
      {
         if (gbSgbMode && gbSgbMultiplayer)
         {
            gbMemory[REG_P1] = 0xf0 | gbSgbNextController;
         }
         else
         {
            gbMemory[REG_P1] = 0xff;
         }
      }
      return gbMemory[REG_P1];
   }
   case 0x07:
      return (0xf8 | gbMemory[REG_TAC]);
   case 0x0f:
      return (0xe0 | gbMemory[IF_FLAG]);
   case 0x10:
   case 0x11:
   case 0x12:
   case 0x13:
   case 0x14:
   case 0x15:
   case 0x16:
   case 0x17:
   case 0x18:
   case 0x19:
   case 0x1a:
   case 0x1b:
   case 0x1c:
   case 0x1d:
   case 0x1e:
   case 0x1f:
   case 0x20:
   case 0x21:
   case 0x22:
   case 0x23:
   case 0x24:
   case 0x25:
   case 0x26:
   case 0x27:
   case 0x28:
   case 0x29:
   case 0x2a:
   case 0x2b:
   case 0x2c:
   case 0x2d:
   case 0x2e:
   case 0x2f:
   case 0x30:
   case 0x31:
   case 0x32:
   case 0x33:
   case 0x34:
   case 0x35:
   case 0x36:
   case 0x37:
   case 0x38:
   case 0x39:
   case 0x3a:
   case 0x3b:
   case 0x3c:
   case 0x3d:
   case 0x3e:
   case 0x3f:
      return gbSoundRead(address);
   case 0x40:
      return register_LCDC;
   case 0x41:
      // This is a GB/C only bug (ie. not GBA/SP).
      if ((gbHardware & 7) && (gbLcdMode == 2) && (gbLcdModeDelayed == 1) && (!gbSpeed))
         return (0x80 | (gbMemory[REG_STAT] & 0xFC));
      else
         return (0x80 | gbMemory[REG_STAT]);
   case 0x44:
      if (((gbHardware & 7) && ((gbLcdMode == 1) && (gbLcdTicks == 0x71))) || (!(register_LCDC & 0x80)))
         return 0;
      else
         return register_LY;
   case 0x01:
   case 0x02:
   case 0x04:
   case 0x05:
   case 0x06:
   case 0x42:
   case 0x43:
   case 0x45:
   case 0x46:
   case 0x47:
   case 0x48:
   case 0x49:
   case 0x4a:
   case 0x4b:
      // read direct from memory
      break;
   case 0xff:
      return register_IE;
   default:
      if (gbCgbMode)
      {
         switch (address & 0x00ff)
         {
         case 0x4f:
            return (0xfe | gbMemory[REG_VBK]);
         case 0x68:
         case 0x6a:
            return (0x40 | gbMemory[address]);
         case 0x69:
         case 0x6b:
            if (gbCgbPaletteAccessValid())
               return (gbMemory[address]);
            break;
         case 0x70:
            return (0xf8 | gbMemory[REG_SVBK]);
         case 0x4d:
         case 0x51:
         case 0x52:
         case 0x53:
         case 0x54:
         case 0x55:
            goto done;
         }
      }
      log("Unknown register read %04x PC=%04x\n",
          address,
          PC.W);
      return 0xFF;
   }

done:
   return gbMemoryMap[address >> 12][address & 0x0fff];
}

void gbWriteMemory(uint16_t address, uint8_t value)
{
   switch (address >> 12)
   {
   // ROM
   case GB_MEM_CART_BANK0:
   case GB_MEM_CART_BANK0 + 1:
   case GB_MEM_CART_BANK0 + 2:
   case GB_MEM_CART_BANK0 + 3:
   case GB_MEM_CART_BANK1:
   case GB_MEM_CART_BANK1 + 1:
   case GB_MEM_CART_BANK1 + 2:
   case GB_MEM_CART_BANK1 + 3:
      mapperWrite(address, value);
      return;

   // VRAM
   case GB_MEM_VRAM:
   case GB_MEM_VRAM + 1:
      if (gbVramWriteAccessValid())
         gbMemoryMap[address >> 12][address & 0x0fff] = value;
      return;

   // external RAM
   case GB_MEM_EXTERNAL_RAM:
   case GB_MEM_EXTERNAL_RAM + 1:
      // Is that a correct fix ??? (it used to be 'if (mapper)')...
      mapperWrite(address, value);
      return;

   // WRAM Bank0/1
   case GB_MEM_WRAM_BANK0:
   case GB_MEM_WRAM_BANK1:
      gbMemoryMap[address >> 12][address & 0x0fff] = value;
      return;

   case GB_MEM_BASE_E:
   case GB_MEM_BASE_F:
      if (address < GB_ADDRESS_OAM)
      {
         // Used for the mirroring of 0xC000 in 0xE000
         address &= ~0x2000;
         gbMemoryMap[address >> 12][address & 0x0fff] = value;
      }
      else if (address < GB_ADDRESS_UNUSUABLE)
      {
         // OAM not accessible during mode 2 & 3.
         if (((gbHardware & 0xa) && ((gbLcdMode | gbLcdModeDelayed) & 2)) || ((gbHardware & 5) && (((gbLcdModeDelayed == 2) && (gbLcdTicksDelayed <= GBLCD_MODE_2_CLOCK_TICKS)) || (gbLcdModeDelayed == 3))))
         {
            return;
         }
         gbMemoryMap[address >> 12][address & 0x0fff] = value;
      }
      else if (address < GB_ADDRESS_IO)
      {
         // UNUSABLE: GBC allows reading/writing to this area
         gbMemoryMap[address >> 12][address & 0x0fff] = value;
      }
      else if (address < GB_ADDRESS_HRAM)
      {
         gbWriteIO(address, value);
      }
      else if (address < GB_ADDRESS_IE)
      {
         // high RAM
         gbMemoryMap[address >> 12][address & 0x0fff] = value;
      }
      else
      {
         // IE
         gbWriteIO(GB_ADDRESS_IE, value);
      }
   }
}

uint8_t gbReadMemory(uint16_t address)
{
   if (gbCheatMap[address])
      return gbCheatRead(address);

   switch (address >> 12)
   {
   case GB_MEM_CART_BANK0:
   case GB_MEM_CART_BANK0 + 1:
   case GB_MEM_CART_BANK0 + 2:
   case GB_MEM_CART_BANK0 + 3:
   case GB_MEM_CART_BANK1:
   case GB_MEM_CART_BANK1 + 1:
   case GB_MEM_CART_BANK1 + 2:
   case GB_MEM_CART_BANK1 + 3:
      return gbMemoryMap[address >> 12][address & 0x0fff];

   case GB_MEM_VRAM:
   case GB_MEM_VRAM + 1:
      if (gbVramReadAccessValid())
         return gbMemoryMap[address >> 12][address & 0x0fff];
      return 0xff;

   case GB_MEM_EXTERNAL_RAM:
   case GB_MEM_EXTERNAL_RAM + 1:
      // for the 2kb ram limit (fixes crash in shawu's story
      // but now its sram test fails, as the it expects 8kb and not 2kb...
      // So use the 'genericflashcard' option to fix it).
      if (address <= (0xa000 + gbRamSizeMask))
      {
         if (mapperRam)
            return mapperReadRAM(address);
         return gbMemoryMap[address >> 12][address & 0x0fff];
      }
      return 0xff;

   // WRAM bank0/1
   case GB_MEM_WRAM_BANK0:
   case GB_MEM_WRAM_BANK1:
      return gbMemoryMap[address >> 12][address & 0x0fff];

   case GB_MEM_BASE_E:
   case GB_MEM_BASE_F:
      if (address < GB_ADDRESS_OAM)
      {
         // Used for the mirroring of 0xC000 in 0xE000
         address &= ~0x2000;
         return gbMemoryMap[address >> 12][address & 0x0fff];
      }
      if (address < GB_ADDRESS_UNUSUABLE)
      {
         // OAM not accessible during mode 2 & 3.
         if (((address >= 0xfe00) && (address < 0xfea0)) && ((((gbLcdMode | gbLcdModeDelayed) & 2) && (!(gbSpeed && (gbHardware & 0x2) && !(gbLcdModeDelayed & 2) && (gbLcdMode == 2)))) || (gbSpeed && (gbHardware & 0x2) && (gbLcdModeDelayed == 0) && (gbLcdTicksDelayed == (GBLCD_MODE_0_CLOCK_TICKS - gbSpritesTicks[299])))))
         {
            return 0xff;
         }
         return gbMemoryMap[address >> 12][address & 0x0fff];
      }
      if (address < GB_ADDRESS_IO)
      {
         if (gbHardware & 1)
            return ((((address + ((address >> 4) - 0xfea)) >> 2) & 1) ? 0x00 : 0xff);
         else if (gbHardware & 2)
            return gbMemoryMap[address >> 12][address & 0x0fff];
         else if (gbHardware & 4)
            return ((((address + ((address >> 4) - 0xfea)) >> 2) & 1) ? 0xff : 0x00);
         else if (gbHardware & 8)
            return ((address & 0xf0) | ((address & 0xf0) >> 4));
      }
      if (address < GB_ADDRESS_HRAM)
      {
         return gbReadIO(address);
      }
      if (address < GB_ADDRESS_IE)
      {
         return gbMemoryMap[address >> 12][address & 0x0fff];
      }
      return gbReadIO(GB_ADDRESS_IE);
   }

   return gbMemoryMap[address >> 12][address & 0x0fff];
}
