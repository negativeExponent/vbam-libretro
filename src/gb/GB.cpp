//#include "../win32/stdafx.h" // would fix LNK2005 linker errors for MSVC
#include <assert.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../NLS.h"
#include "../System.h"
#include "../Util.h"
#include "../common/ConfigManager.h"
#include "../gba/GBALink.h"
#include "../gba/Sound.h"
#include "gb.h"
#include "gbCheats.h"
#include "gbConstants.h"
#include "gbGlobals.h"
#include "gbMemory.h"
#include "gbSGB.h"
#include "gbSound.h"

#ifdef __GNUC__
#define _stricmp strcasecmp
#endif

extern pixFormat* pix;
bool gbUpdateSizes();
bool inBios = false;

// debugging
bool memorydebug = false;
char gbBuffer[2048];

extern uint16_t gbLineMix[160];

// mappers
int mapperType = MBC1;
bool mapperRam = false;

// registers
gbRegister PC;
gbRegister SP;
gbRegister AF;
gbRegister BC;
gbRegister DE;
gbRegister HL;
uint16_t IFF = 0;
// 0xff0f
uint8_t register_IF = 0;
// 0xff40
uint8_t register_LCDC = 0;
// 0xff41
uint8_t register_STAT = 0;
// 0xff44
uint8_t register_LY = 0;
// 0xffff
uint8_t register_IE = 0;

// ticks definition
int GBDIV_CLOCK_TICKS = 64;
int GBLCD_MODE_0_CLOCK_TICKS = 51;
int GBLCD_MODE_1_CLOCK_TICKS = 1140;
int GBLCD_MODE_2_CLOCK_TICKS = 20;
int GBLCD_MODE_3_CLOCK_TICKS = 43;
int GBLY_INCREMENT_CLOCK_TICKS = 114;
int GBTIMER_MODE_0_CLOCK_TICKS = 256;
int GBTIMER_MODE_1_CLOCK_TICKS = 4;
int GBTIMER_MODE_2_CLOCK_TICKS = 16;
int GBTIMER_MODE_3_CLOCK_TICKS = 64;
int GBSERIAL_CLOCK_TICKS = 128;
int GBSYNCHRONIZE_CLOCK_TICKS = 52920;

// state variables

// general
int clockTicks = 0;
bool gbSystemMessage = false;
int gbGBCColorType = 0;
int gbHardware = 0;
int gbRomType = 0;
int gbRemainingClockTicks = 0;
int gbOldClockTicks = 0;
int gbIntBreak = 0;
int gbInterruptLaunched = 0;

// breakpoint
bool breakpoint = false;
// interrupt
int gbInt48Signal = 0;
int gbInterruptWait = 0;
// serial
int gbSerialOn = 0;
int gbSerialTicks = 0;
int gbSerialBits = 0;
// timer
int gbTimerOn = 0;
int gbTimerTicks = GBTIMER_MODE_0_CLOCK_TICKS;
int gbTimerClockTicks = GBTIMER_MODE_0_CLOCK_TICKS;
int gbTimerMode = 0;
bool gbIncreased = false;
// The internal timer is always active, and it is
// not reset by writing to register TIMA/TMA, but by
// writing to register DIV...
int gbInternalTimer = 0x55;
const uint8_t gbTimerMask[4] = { 0xff, 0x3, 0xf, 0x3f };
const uint8_t gbTimerBug[8] = { 0x80, 0x80, 0x02, 0x02, 0x0, 0xff, 0x0, 0xff };
bool gbTimerModeChange = false;
bool gbTimerOnChange = false;
// lcd
bool gbScreenOn = true;
int gbLcdMode = 2;
int gbLcdModeDelayed = 2;
int gbLcdTicks = GBLCD_MODE_2_CLOCK_TICKS - 1;
int gbLcdTicksDelayed = GBLCD_MODE_2_CLOCK_TICKS;
int gbLcdLYIncrementTicks = 114;
int gbLcdLYIncrementTicksDelayed = 115;
int gbScreenTicks = 0;
uint8_t gbSCYLine[300];
uint8_t gbSCXLine[300];
uint8_t gbBgpLine[300];
uint8_t gbObp0Line[300];
uint8_t gbObp1Line[300];
uint8_t gbSpritesTicks[300];
uint8_t oldRegister_WY;
bool gbLYChangeHappened = false;
bool gbLCDChangeHappened = false;
int gbLine99Ticks = 1;
int gbRegisterLYLCDCOffOn = 0;
int inUseRegister_WY = 0;

// Used to keep track of the line that ellapse
// when screen is off
int gbWhiteScreen = 0;
bool gbBlackScreen = false;
int register_LCDCBusy = 0;

// div
int gbDivTicks = GBDIV_CLOCK_TICKS;
// cgb
int gbVramBank = 0;
int gbWramBank = 1;
//sgb
bool gbSgbResetFlag = false;
// gbHdmaDestination is 0x99d0 on startup (tested on HW)
// but I'm not sure what gbHdmaSource is...
int gbHdmaSource = 0x99d0;
int gbHdmaDestination = 0x99d0;
int gbHdmaBytes = 0x0000;
int gbHdmaOn = 0;
int gbSpeed = 0;
// frame counting
int gbFrameCount = 0;
int gbFrameSkip = 0;
int gbFrameSkipCount = 0;
// timing
uint32_t gbLastTime = 0;
uint32_t gbElapsedTime = 0;
uint32_t gbTimeNow = 0;
int gbSynchronizeTicks = GBSYNCHRONIZE_CLOCK_TICKS;
// emulator features
bool gbBattery = false;
bool gbRumble = false;
bool gbRTCPresent = false;
bool gbBatteryError = false;
int gbCaptureNumber = 0;
bool gbCapture = false;
bool gbCapturePrevious = false;
int gbJoymask[4] = { 0, 0, 0, 0 };
static bool allow_colorizer_hack;

#define GBSAVE_GAME_VERSION_1 1
#define GBSAVE_GAME_VERSION_2 2
#define GBSAVE_GAME_VERSION_3 3
#define GBSAVE_GAME_VERSION_4 4
#define GBSAVE_GAME_VERSION_5 5
#define GBSAVE_GAME_VERSION_6 6
#define GBSAVE_GAME_VERSION_7 7
#define GBSAVE_GAME_VERSION_8 8
#define GBSAVE_GAME_VERSION_9 9
#define GBSAVE_GAME_VERSION_10 10
#define GBSAVE_GAME_VERSION_11 11
#define GBSAVE_GAME_VERSION_12 12
#define GBSAVE_GAME_VERSION GBSAVE_GAME_VERSION_12

void setColorizerHack(bool value)
{
   allow_colorizer_hack = value;
}

bool allowColorizerHack(void)
{
   if (gbHardware & 0xA)
      return (allow_colorizer_hack);
   return false;
}

static inline bool gbVramReadAccessValid(void)
{
   // A lot of 'ugly' checks... But only way to emulate this particular behaviour...
   if (allowColorizerHack() || ((gbHardware & 0xa) && ((gbLcdModeDelayed != 3) || (((register_LY == 0) && (gbScreenOn == false) && (register_LCDC & 0x80)) && (gbLcdLYIncrementTicksDelayed == (GBLY_INCREMENT_CLOCK_TICKS - GBLCD_MODE_2_CLOCK_TICKS))))) || ((gbHardware & 0x5) && (gbLcdModeDelayed != 3) && ((gbLcdMode != 3) || ((register_LY == 0) && ((gbScreenOn == false) && (register_LCDC & 0x80)) && (gbLcdLYIncrementTicks == (GBLY_INCREMENT_CLOCK_TICKS - GBLCD_MODE_2_CLOCK_TICKS))))))
      return true;
   return false;
}

static inline bool gbVramWriteAccessValid(void)
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

static inline bool gbCgbPaletteAccessValid(void)
{
   // No access to gbPalette during mode 3 (Color Panel Demo)
   if (allowColorizerHack() || ((gbLcdModeDelayed != 3) && (!((gbLcdMode == 0) && (gbLcdTicks >= (GBLCD_MODE_0_CLOCK_TICKS - gbSpritesTicks[299] - 1)))) && (!gbSpeed)) || (gbSpeed && ((gbLcdMode == 1) || (gbLcdMode == 2) || ((gbLcdMode == 3) && (gbLcdTicks > (GBLCD_MODE_3_CLOCK_TICKS - 2))) || ((gbLcdMode == 0) && (gbLcdTicks <= (GBLCD_MODE_0_CLOCK_TICKS - gbSpritesTicks[299] - 2))))))
      return true;
   return false;
}

int inline gbGetValue(int min, int max, int v)
{
   return (int)(min + (float)(max - min) * (2.0 * (v / 31.0) - (v / 31.0) * (v / 31.0)));
}

void gbGenFilter()
{
   for (int r = 0; r < 32; r++)
   {
      for (int g = 0; g < 32; g++)
      {
         for (int b = 0; b < 32; b++)
         {
            int nr = gbGetValue(gbGetValue(4, 14, g), gbGetValue(24, 29, g), r) - 4;
            int ng = gbGetValue(gbGetValue(4 + gbGetValue(0, 5, r), 14 + gbGetValue(0, 3, r), b),
                         gbGetValue(24 + gbGetValue(0, 3, r), 29 + gbGetValue(0, 1, r), b), g)
                - 4;
            int nb = gbGetValue(gbGetValue(4 + gbGetValue(0, 5, r), 14 + gbGetValue(0, 3, r), g),
                         gbGetValue(24 + gbGetValue(0, 3, r), 29 + gbGetValue(0, 1, r), g), b)
                - 4;
            gbColorFilter[(b << 10) | (g << 5) | r] = (nb << 10) | (ng << 5) | nr;
         }
      }
   }
}

bool gbIsGameboyRom(char* file)
{
   if (strlen(file) > 4)
   {
      char* p = strrchr(file, '.');

      if (p != NULL)
      {
         if (_stricmp(p, ".gb") == 0)
            return true;
         if (_stricmp(p, ".dmg") == 0)
            return true;
         if (_stricmp(p, ".gbc") == 0)
            return true;
         if (_stricmp(p, ".cgb") == 0)
            return true;
         if (_stricmp(p, ".sgb") == 0)
            return true;
      }
   }

   return false;
}

void gbCopyMemory(uint16_t d, uint16_t s, int count)
{
   while (count)
   {
      gbMemoryMap[d >> 12][d & 0x0fff] = gbMemoryMap[s >> 12][s & 0x0fff];
      s++;
      d++;
      count--;
   }
}

void gbDoHdma()
{

   gbCopyMemory((gbHdmaDestination & 0x1ff0) | 0x8000,
       gbHdmaSource & 0xfff0,
       0x10);

   gbHdmaDestination += 0x10;
   if (gbHdmaDestination == 0xa000)
      gbHdmaDestination = 0x8000;

   gbHdmaSource += 0x10;
   if (gbHdmaSource == 0x8000)
      gbHdmaSource = 0xa000;

   gbMemory[REG_HDMA2] = gbHdmaSource & 0xff;
   gbMemory[REG_HDMA1] = gbHdmaSource >> 8;

   gbMemory[REG_HDMA4] = gbHdmaDestination & 0xff;
   gbMemory[REG_HDMA3] = gbHdmaDestination >> 8;

   gbHdmaBytes -= 0x10;
   gbMemory[0xff55]--;
   if (gbMemory[REG_HDMA5] == 0xff)
      gbHdmaOn = 0;

   // We need to add the dmaClockticks for HDMA !
   if (gbSpeed)
      gbDmaTicks = 17;
   else
      gbDmaTicks = 9;

   if (IFF & 0x80)
      gbDmaTicks++;
}

// fix for Harley and Lego Racers
void gbCompareLYToLYC()
{
   if (register_LCDC & 0x80)
   {
      if (register_LY == gbMemory[REG_LYC])
      {
         // mark that we have a match
         register_STAT |= 4;

         // check if we need an interrupt
         if (register_STAT & 0x40)
         {
            // send LCD interrupt only if no interrupt 48h signal...
            if (!gbInt48Signal)
            {
               register_IF |= 2;
            }
            gbInt48Signal |= 8;
         }
      }
      else // no match
      {
         register_STAT &= 0xfb;
         gbInt48Signal &= ~8;
      }
   }
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
         gbSerialOn        = 0;
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

            gbRegisterLYLCDCOffOn = (register_LY + 144) % 154;

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

      if ((gbHardware & 7)
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
            memcpy((uint8_t*)(gbRom + 0x100), (uint8_t*)(gbMemory + 0x100), 0xF00);
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
         int joystate = gbJoymask[joy];
         if (!(joystate & 0x80)) P1 |= 0x08;
         if (!(joystate & 0x40)) P1 |= 0x04;
         if (!(joystate & 0x20)) P1 |= 0x02;
         if (!(joystate & 0x10)) P1 |= 0x01;
         gbMemory[REG_P1] = P1;
      }
      else if ((P1 & 0x30) == 0x10)
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
         int joystate = gbJoymask[joy];
         if (!(joystate & 0x08)) P1 |= 0x08;
         if (!(joystate & 0x04)) P1 |= 0x04;
         if (!(joystate & 0x02)) P1 |= 0x02;
         if (!(joystate & 0x01)) P1 |= 0x01;
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
         if (((gbHardware & 0xa) && ((gbLcdMode | gbLcdModeDelayed) & 2)) ||
               ((gbHardware & 5) && (((gbLcdModeDelayed == 2) && (gbLcdTicksDelayed <= GBLCD_MODE_2_CLOCK_TICKS)) ||
               (gbLcdModeDelayed == 3))))
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
         if (((address >= 0xfe00) && (address < 0xfea0)) &&
               ((((gbLcdMode | gbLcdModeDelayed) & 2) && (!(gbSpeed && (gbHardware & 0x2) && !(gbLcdModeDelayed & 2) && (gbLcdMode == 2)))) ||
               (gbSpeed && (gbHardware & 0x2) && (gbLcdModeDelayed == 0) && (gbLcdTicksDelayed == (GBLCD_MODE_0_CLOCK_TICKS - gbSpritesTicks[299])))))
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

void gbVblank_interrupt()
{
   gbCheatWrite(false); // Emulates GS codes.
   gbMemory[IF_FLAG] = register_IF &= 0xfe;
   gbWriteMemory(--SP.W, PC.B.B1);
   gbWriteMemory(--SP.W, PC.B.B0);
   PC.W = 0x40;
}

void gbLcd_interrupt()
{
   gbCheatWrite(false); // Emulates GS codes.
   gbMemory[IF_FLAG] = register_IF &= 0xfd;
   gbWriteMemory(--SP.W, PC.B.B1);
   gbWriteMemory(--SP.W, PC.B.B0);
   PC.W = 0x48;
}

void gbTimer_interrupt()
{
   gbMemory[IF_FLAG] = register_IF &= 0xfb;
   gbWriteMemory(--SP.W, PC.B.B1);
   gbWriteMemory(--SP.W, PC.B.B0);
   PC.W = 0x50;
}

void gbSerial_interrupt()
{
   gbMemory[IF_FLAG] = register_IF &= 0xf7;
   gbWriteMemory(--SP.W, PC.B.B1);
   gbWriteMemory(--SP.W, PC.B.B0);
   PC.W = 0x58;
}

void gbJoypad_interrupt()
{
   gbMemory[IF_FLAG] = register_IF &= 0xef;
   gbWriteMemory(--SP.W, PC.B.B1);
   gbWriteMemory(--SP.W, PC.B.B0);
   PC.W = 0x60;
}

void gbSpeedSwitch()
{
   gbBlackScreen = true;
   if (gbSpeed == 0)
   {
      gbSpeed = 1;
      GBLCD_MODE_0_CLOCK_TICKS = 51 * 2;
      GBLCD_MODE_1_CLOCK_TICKS = 1140 * 2;
      GBLCD_MODE_2_CLOCK_TICKS = 20 * 2;
      GBLCD_MODE_3_CLOCK_TICKS = 43 * 2;
      GBLY_INCREMENT_CLOCK_TICKS = 114 * 2;
      GBDIV_CLOCK_TICKS = 64;
      GBTIMER_MODE_0_CLOCK_TICKS = 256;
      GBTIMER_MODE_1_CLOCK_TICKS = 4;
      GBTIMER_MODE_2_CLOCK_TICKS = 16;
      GBTIMER_MODE_3_CLOCK_TICKS = 64;
      GBSERIAL_CLOCK_TICKS = 128 * 2;
      gbLcdTicks *= 2;
      gbLcdTicksDelayed *= 2;
      gbLcdTicksDelayed--;
      gbLcdLYIncrementTicks *= 2;
      gbLcdLYIncrementTicksDelayed *= 2;
      gbLcdLYIncrementTicksDelayed--;
      gbSerialTicks *= 2;
      gbLine99Ticks = 3;
   }
   else
   {
      gbSpeed = 0;
      GBLCD_MODE_0_CLOCK_TICKS = 51;
      GBLCD_MODE_1_CLOCK_TICKS = 1140;
      GBLCD_MODE_2_CLOCK_TICKS = 20;
      GBLCD_MODE_3_CLOCK_TICKS = 43;
      GBLY_INCREMENT_CLOCK_TICKS = 114;
      GBDIV_CLOCK_TICKS = 64;
      GBTIMER_MODE_0_CLOCK_TICKS = 256;
      GBTIMER_MODE_1_CLOCK_TICKS = 4;
      GBTIMER_MODE_2_CLOCK_TICKS = 16;
      GBTIMER_MODE_3_CLOCK_TICKS = 64;
      GBSERIAL_CLOCK_TICKS = 128;
      gbLcdTicks >>= 1;
      gbLcdTicksDelayed++;
      gbLcdTicksDelayed >>= 1;
      gbLcdLYIncrementTicks >>= 1;
      gbLcdLYIncrementTicksDelayed++;
      gbLcdLYIncrementTicksDelayed >>= 1;
      gbSerialTicks /= 2;
      gbLine99Ticks = 1;
      if (gbHardware & 8)
         gbLine99Ticks++;
   }
   gbDmaTicks += (134) * GBLY_INCREMENT_CLOCK_TICKS + (37 << (gbSpeed ? 1 : 0));
}

bool CPUIsGBBios(const char* file)
{
   if (strlen(file) > 4)
   {
      const char* p = strrchr(file, '.');

      if (p != NULL)
      {
         if (_stricmp(p, ".gb") == 0)
            return true;
         if (_stricmp(p, ".bin") == 0)
            return true;
         if (_stricmp(p, ".bios") == 0)
            return true;
         if (_stricmp(p, ".rom") == 0)
            return true;
      }
   }

   return false;
}

void gbCPUInit(const char* biosFileName, bool useBiosFile)
{
   // GB/GBC/SGB only at the moment
   if (!(gbHardware & 7))
      return;

   useBios = false;
   if (useBiosFile)
   {
      int expectedSize = (gbHardware & 2) ? 0x900 : 0x100;
      int size = expectedSize;
      if (utilLoad(biosFileName,
              CPUIsGBBios,
              bios,
              size))
      {
         if (size == expectedSize)
            useBios = true;
         else
            systemMessage(MSG_INVALID_BIOS_FILE_SIZE, N_("Invalid BOOTROM file size"));
      }
   }
}

void gbGetHardwareType()
{
   gbCgbMode = 0;
   gbSgbMode = 0;
   if ((gbEmulatorType == 0 && (gbRom[0x143] & 0x80)) || gbEmulatorType == 1 || gbEmulatorType == 4)
   {
      gbCgbMode = 1;
   }

   if ((gbCgbMode == 0) && (gbRom[0x146] == 0x03))
   {
      if (gbEmulatorType == 0 || gbEmulatorType == 2 || gbEmulatorType == 5)
         gbSgbMode = 1;
   }

   gbHardware = 1; // GB
   if (((gbCgbMode == 1) && (gbEmulatorType == 0)) || (gbEmulatorType == 1))
      gbHardware = 2; // GBC
   else if (((gbSgbMode == 1) && (gbEmulatorType == 0)) || (gbEmulatorType == 2) || (gbEmulatorType == 5))
      gbHardware = 4; // SGB(2)
   else if (gbEmulatorType == 4)
      gbHardware = 8; // GBA

   gbGBCColorType = 0;
   if (gbHardware & 8) // If GBA is selected, choose the GBA default settings.
      gbGBCColorType = 2; // (0 = GBC, 1 = GBA, 2 = GBASP)
}

static void gbSelectColorizationPalette()
{
   int infoIdx = 0;

   // Check if licensee is Nintendo. If not, use default palette.
   if (gbRom[0x014B] == 0x01 || (gbRom[0x014B] == 0x33 && gbRom[0x0144] == 0x30 && gbRom[0x0145] == 0x31))
   {
      // Calculate the checksum over 16 title bytes.
      uint8_t checksum = 0;
      for (int i = 0; i < 16; i++)
      {
         checksum += gbRom[0x0134 + i];
      }

      // Check if the checksum is in the list.
      size_t idx;
      for (idx = 0; idx < sizeof(gbColorizationChecksums); idx++)
      {
         if (gbColorizationChecksums[idx] == checksum)
         {
            break;
         }
      }

      // Was the checksum found in the list?
      if (idx < sizeof(gbColorizationChecksums))
      {
         // Indexes above 0x40 have to be disambiguated.
         if (idx > 0x40)
         {
            // No idea how that works. But it works.
            for (size_t i = idx - 0x41, j = 0; i < sizeof(gbColorizationDisambigChars); i += 14, j += 14)
            {
               if (gbRom[0x0137] == gbColorizationDisambigChars[i])
               {
                  infoIdx = idx + j;
                  break;
               }
            }
         }
         else
         {
            // Lower indexes just use the index from the checksum list.
            infoIdx = idx;
         }
      }
   }
   uint8_t palette = gbColorizationPaletteInfo[infoIdx] & 0x1F;
   uint8_t flags = (gbColorizationPaletteInfo[infoIdx] & 0xE0) >> 5;

   // Normally the first palette is used as OBP0.
   // If bit 0 is zero, the third palette is used instead.
   const uint16_t* obp0 = 0;
   if (flags & 1)
   {
      obp0 = gbColorizationPaletteData[palette][0];
   }
   else
   {
      obp0 = gbColorizationPaletteData[palette][2];
   }

   memcpy(gbPalette + 32, obp0, sizeof(gbColorizationPaletteData[palette][0]));

   // Normally the second palette is used as OBP1.
   // If bit 1 is set, the first palette is used instead.
   // If bit 2 is zero, the third palette is used instead.
   const uint16_t* obp1 = 0;
   if (!(flags & 4))
   {
      obp1 = gbColorizationPaletteData[palette][2];
   }
   else if (flags & 2)
   {
      obp1 = gbColorizationPaletteData[palette][0];
   }
   else
   {
      obp1 = gbColorizationPaletteData[palette][1];
   }

   memcpy(gbPalette + 36, obp1, sizeof(gbColorizationPaletteData[palette][0]));

   // Third palette is always used for BGP.
   memcpy(gbPalette, gbColorizationPaletteData[palette][2], sizeof(gbColorizationPaletteData[palette][0]));
}

void gbReset()
{
#ifndef NO_LINK
   if (GetLinkMode() == LINK_GAMEBOY_IPC || GetLinkMode() == LINK_GAMEBOY_SOCKET)
   {
      EmuReseted = true;
      gbInitLink();
   }
#endif // NO_LINK

   gbGetHardwareType();

   oldRegister_WY = 146;
   gbInterruptLaunched = 0;

   if (gbCgbMode == 1)
   {
      if (gbVram == NULL)
         gbVram = (uint8_t*)malloc(0x4000);
      if (gbWram == NULL)
         gbWram = (uint8_t*)malloc(0x8000);
      memset(gbVram, 0, 0x4000);
      memset(gbPalette, 0, 2 * 128);
   }
   else
   {
      if (gbVram != NULL)
      {
         free(gbVram);
         gbVram = NULL;
      }
      if (gbWram != NULL)
      {
         free(gbWram);
         gbWram = NULL;
      }
   }

   gbLYChangeHappened = false;
   gbLCDChangeHappened = false;
   gbBlackScreen = false;
   gbInterruptWait = 0;
   gbDmaTicks = 0;
   clockTicks = 0;

   // clean Wram
   // This kinda emulates the startup state of Wram on GB/C (not very accurate,
   // but way closer to the reality than filling it with 00es or FFes).
   // On GBA/GBASP, it's kinda filled with random data.
   // In all cases, most of the 2nd bank is filled with 00s.
   // The starting data are important for some 'buggy' games, like Buster Brothers or
   // Karamuchou ha Oosawagi!.
   if (gbMemory != NULL)
   {
      memset(gbMemory, 0xff, 65536);
      for (int temp = 0xC000; temp < 0xE000; temp++)
         if ((temp & 0x8) ^ ((temp & 0x800) >> 8))
         {
            if ((gbHardware & 0x02) && (gbGBCColorType == 0))
               gbMemory[temp] = 0x0;
            else
               gbMemory[temp] = 0x0f;
         }

         else
            gbMemory[temp] = 0xff;
   }

   if (gbSpeed)
   {
      gbSpeedSwitch();
      gbMemory[REG_KEY1] = 0;
   }

   // GB bios set this memory area to 0
   // Fixes Pitman (J) title screen
   if (gbHardware & 0x1)
   {
      memset(&gbMemory[0x8000], 0x0, 0x2000);
   }

   // clean LineBuffer
   if (gbLineBuffer != NULL)
      memset(gbLineBuffer, 0, sizeof(*gbLineBuffer));
   // clean Pix
   if (pix != NULL)
      memset(pix, 0, sizeof(*pix));
   // clean Vram
   if (gbVram != NULL)
      memset(gbVram, 0, 0x4000);
   // clean Wram 2
   // This kinda emulates the startup state of Wram on GBC (not very accurate,
   // but way closer to the reality than filling it with 00es or FFes).
   // On GBA/GBASP, it's kinda filled with random data.
   // In all cases, most of the 2nd bank is filled with 00s.
   // The starting data are important for some 'buggy' games, like Buster Brothers or
   // Karamuchou ha Oosawagi!
   if (gbWram != NULL)
   {
      for (int i = 0; i < 8; i++)
         if (i != 2)
            memcpy((uint16_t*)(gbWram + i * 0x1000), (uint16_t*)(gbMemory + 0xC000), 0x1000);
   }

   memset(gbSCYLine, 0, sizeof(gbSCYLine));
   memset(gbSCXLine, 0, sizeof(gbSCXLine));
   memset(gbBgpLine, 0xfc, sizeof(gbBgpLine));
   if (gbHardware & 5)
   {
      memset(gbObp0Line, 0xff, sizeof(gbObp0Line));
      memset(gbObp1Line, 0xff, sizeof(gbObp1Line));
   }
   else
   {
      memset(gbObp0Line, 0x0, sizeof(gbObp0Line));
      memset(gbObp1Line, 0x0, sizeof(gbObp1Line));
   }
   memset(gbSpritesTicks, 0x0, sizeof(gbSpritesTicks));

   SP.W = 0xfffe;
   AF.W = 0x01b0;
   BC.W = 0x0013;
   DE.W = 0x00d8;
   HL.W = 0x014d;
   PC.W = 0x0100;
   IFF = 0;
   gbInt48Signal = 0;

   gbMemory[REG_TIMA] = 0;
   gbMemory[REG_TMA] = 0;
   gbMemory[REG_TAC] = 0;
   gbMemory[IF_FLAG] = register_IF = 1;
   gbMemory[REG_LCDC] = register_LCDC = 0x91;
   gbMemory[REG_BGP] = 0xfc;

   if (gbCgbMode)
      gbMemory[REG_KEY1] = 0x7e;
   else
      gbMemory[REG_KEY1] = 0xff;

   if (!gbCgbMode)
      gbMemory[0xff70] = gbMemory[0xff74] = 0xff;

   if (gbCgbMode)
      gbMemory[0xff56] = 0x3e;
   else
      gbMemory[0xff56] = 0xff;

   gbMemory[REG_SCY] = 0;
   gbMemory[REG_SCX] = 0;
   gbMemory[REG_LYC] = 0;
   gbMemory[REG_DMA] = 0xff;
   gbMemory[REG_WY] = 0;
   gbMemory[REG_WX] = 0;
   gbMemory[REG_VBK] = 0;
   gbMemory[REG_HDMA1] = 0xff;
   gbMemory[REG_HDMA2] = 0xff;
   gbMemory[REG_HDMA3] = 0xff;
   gbMemory[REG_HDMA4] = 0xff;
   gbMemory[REG_HDMA5] = 0xff;
   gbMemory[REG_SVBK] = 0;
   register_IE = 0;

   if (gbCgbMode)
      gbMemory[REG_SC] = 0x7c;
   else
      gbMemory[REG_SC] = 0x7e;

   gbMemory[0xff03] = 0xff;

   for (int i = 0x8; i < 0xf; i++)
      gbMemory[0xff00 + i] = 0xff;

   gbMemory[0xff13] = 0xff;
   gbMemory[0xff15] = 0xff;
   gbMemory[0xff18] = 0xff;
   gbMemory[0xff1d] = 0xff;
   gbMemory[0xff1f] = 0xff;

   for (int i = 0x27; i < 0x30; i++)
      gbMemory[0xff00 + i] = 0xff;

   gbMemory[0xff4c] = 0xff;
   gbMemory[0xff4e] = 0xff;
   gbMemory[0xff50] = 0xff;

   for (int i = 0x57; i < 0x68; i++)
      gbMemory[0xff00 + i] = 0xff;

   for (int i = 0x5d; i < 0x70; i++)
      gbMemory[0xff00 + i] = 0xff;

   gbMemory[0xff71] = 0xff;

   for (int i = 0x78; i < 0x80; i++)
      gbMemory[0xff00 + i] = 0xff;

   if (gbHardware & 0xa)
   {

      if (gbHardware & 2)
      {
         AF.W = 0x1180;
         BC.W = 0x0000;
      }
      else
      {
         AF.W = 0x1100;
         BC.W = 0x0100; // GBA/SP have B = 0x01 (which means GBC & GBA/SP bootrom are different !)
      }

      gbMemory[0xff26] = 0xf1;
      if (gbCgbMode)
      {

         gbMemory[0xff31] = 0xff;
         gbMemory[0xff33] = 0xff;
         gbMemory[0xff35] = 0xff;
         gbMemory[0xff37] = 0xff;
         gbMemory[0xff39] = 0xff;
         gbMemory[0xff3b] = 0xff;
         gbMemory[0xff3d] = 0xff;

         gbMemory[REG_LY] = register_LY = 0x90;
         gbDivTicks = 0x19 + ((gbHardware & 2) >> 1);
         gbInternalTimer = 0x58 + ((gbHardware & 2) >> 1);
         gbLcdTicks = GBLCD_MODE_1_CLOCK_TICKS - (register_LY - 0x8F) * GBLY_INCREMENT_CLOCK_TICKS + 72 + ((gbHardware & 2) >> 1);
         gbLcdLYIncrementTicks = 72 + ((gbHardware & 2) >> 1);
         gbMemory[REG_DIV] = 0x1E;
      }
      else
      {
         gbMemory[REG_LY] = register_LY = 0x94;
         gbDivTicks = 0x22 + ((gbHardware & 2) >> 1);
         gbInternalTimer = 0x61 + ((gbHardware & 2) >> 1);
         gbLcdTicks = GBLCD_MODE_1_CLOCK_TICKS - (register_LY - 0x8F) * GBLY_INCREMENT_CLOCK_TICKS + 25 + ((gbHardware & 2) >> 1);
         gbLcdLYIncrementTicks = 25 + ((gbHardware & 2) >> 1);
         gbMemory[REG_DIV] = 0x26;
      }

      DE.W = 0xff56;
      HL.W = 0x000d;

      gbMemory[REG_HDMA5] = 0xff;
      gbMemory[REG_BCPS] = 0xc0;
      gbMemory[REG_OCPS] = 0xc0;

      gbMemory[REG_STAT] = register_STAT = 0x81;
      gbLcdMode = 1;
   }
   else
   {
      if (gbHardware & 4)
      {
         if (gbEmulatorType == 5)
            AF.W = 0xffb0;
         else
            AF.W = 0x01b0;
         BC.W = 0x0013;
         DE.W = 0x00d8;
         HL.W = 0x014d;
      }
      gbDivTicks = 14;
      gbInternalTimer = gbDivTicks--;
      gbMemory[REG_DIV] = 0xAB;
      gbMemory[REG_STAT] = register_STAT = 0x85;
      gbMemory[REG_LY] = register_LY = 0x00;
      gbLcdTicks = 15;
      gbLcdLYIncrementTicks = 114 + gbLcdTicks;
      gbLcdMode = 1;
   }

   // used for the handling of the gb Boot Rom
   if ((gbHardware & 7) && (bios != NULL) && useBios && !skipBios)
   {
      if (gbHardware & 5)
      {
         memcpy((uint8_t*)(gbMemory), (uint8_t*)(gbRom), 0x1000);
         memcpy((uint8_t*)(gbMemory), (uint8_t*)(bios), 0x100);
      }
      else
      {
         memcpy((uint8_t*)(gbMemory), (uint8_t*)(bios), 0x900);
         memcpy((uint8_t*)(gbMemory + 0x100), (uint8_t*)(gbRom + 0x100), 0x100);
      }
      gbWhiteScreen = 0;

      gbInternalTimer = 0x3e;
      gbDivTicks = 0x3f;
      gbMemory[REG_DIV] = 0x00;
      PC.W = 0x0000;
      register_LCDC = 0x11;
      gbScreenOn = false;
      gbLcdTicks = 0;
      gbLcdMode = 0;
      gbLcdModeDelayed = 0;
      gbMemory[REG_STAT] = register_STAT &= 0xfc;
      gbInt48Signal = 0;
      gbLcdLYIncrementTicks = GBLY_INCREMENT_CLOCK_TICKS;
      gbMemory[REG_UNK6C] = 0xfe;

      inBios = true;
   }
   else if (gbHardware & 0xa)
   {
      // Set compatibility mode if it is a DMG ROM.
      gbMemory[REG_UNK6C] = 0xfe | (uint8_t) !(gbRom[0x143] & 0x80);
   }

   gbLine99Ticks = 1;
   if (gbHardware & 8)
      gbLine99Ticks++;

   gbLcdModeDelayed = gbLcdMode;
   gbLcdTicksDelayed = gbLcdTicks + 1;
   gbLcdLYIncrementTicksDelayed = gbLcdLYIncrementTicks + 1;

   gbTimerModeChange = false;
   gbTimerOnChange = false;
   gbTimerOn = 0;

   if (gbCgbMode)
   {
      for (int i = 0; i < 0x20; i++)
         gbPalette[i] = 0x7fff;

      // This is just to show that the starting values of the OBJ palettes are different
      // between the 3 consoles, and that they 'kinda' stay the same at each reset
      // (they can slightly change, somehow (randomly?)).
      // You can check the effects of gbGBCColorType on the "Vila Caldan Color" gbc demo.
      // Note that you could also check the Div register to check on which system the game
      // is running (GB,GBC and GBA(SP) have different startup values).
      // Unfortunatly, I don't have any SGB system, so I can't get their starting values.

      if (gbGBCColorType == 0) // GBC Hardware
      {
         gbPalette[0x20] = 0x0600;
         gbPalette[0x21] = 0xfdf3;
         gbPalette[0x22] = 0x041c;
         gbPalette[0x23] = 0xf5db;
         gbPalette[0x24] = 0x4419;
         gbPalette[0x25] = 0x57ea;
         gbPalette[0x26] = 0x2808;
         gbPalette[0x27] = 0x9b75;
         gbPalette[0x28] = 0x129b;
         gbPalette[0x29] = 0xfce0;
         gbPalette[0x2a] = 0x22da;
         gbPalette[0x2b] = 0x4ac5;
         gbPalette[0x2c] = 0x2d71;
         gbPalette[0x2d] = 0xf0c2;
         gbPalette[0x2e] = 0x5137;
         gbPalette[0x2f] = 0x2d41;
         gbPalette[0x30] = 0x6b2d;
         gbPalette[0x31] = 0x2215;
         gbPalette[0x32] = 0xbe0a;
         gbPalette[0x33] = 0xc053;
         gbPalette[0x34] = 0xfe5f;
         gbPalette[0x35] = 0xe000;
         gbPalette[0x36] = 0xbe10;
         gbPalette[0x37] = 0x914d;
         gbPalette[0x38] = 0x7f91;
         gbPalette[0x39] = 0x02b5;
         gbPalette[0x3a] = 0x77ac;
         gbPalette[0x3b] = 0x14e5;
         gbPalette[0x3c] = 0xcf89;
         gbPalette[0x3d] = 0xa03d;
         gbPalette[0x3e] = 0xfd50;
         gbPalette[0x3f] = 0x91ff;
      }
      else if (gbGBCColorType == 1) // GBA Hardware
      {
         gbPalette[0x20] = 0xbe00;
         gbPalette[0x21] = 0xfdfd;
         gbPalette[0x22] = 0xbd69;
         gbPalette[0x23] = 0x7baf;
         gbPalette[0x24] = 0xf5ff;
         gbPalette[0x25] = 0x3f8f;
         gbPalette[0x26] = 0xcee5;
         gbPalette[0x27] = 0x5bf7;
         gbPalette[0x28] = 0xb35b;
         gbPalette[0x29] = 0xef97;
         gbPalette[0x2a] = 0xef9f;
         gbPalette[0x2b] = 0x97f7;
         gbPalette[0x2c] = 0x82bf;
         gbPalette[0x2d] = 0x9f3d;
         gbPalette[0x2e] = 0xddde;
         gbPalette[0x2f] = 0xbad5;
         gbPalette[0x30] = 0x3cba;
         gbPalette[0x31] = 0xdfd7;
         gbPalette[0x32] = 0xedea;
         gbPalette[0x33] = 0xfeda;
         gbPalette[0x34] = 0xf7f9;
         gbPalette[0x35] = 0xfdee;
         gbPalette[0x36] = 0x6d2f;
         gbPalette[0x37] = 0xf0e6;
         gbPalette[0x38] = 0xf7f0;
         gbPalette[0x39] = 0xf296;
         gbPalette[0x3a] = 0x3bf1;
         gbPalette[0x3b] = 0xe211;
         gbPalette[0x3c] = 0x69ba;
         gbPalette[0x3d] = 0x3d0d;
         gbPalette[0x3e] = 0xdfd3;
         gbPalette[0x3f] = 0xa6ba;
      }
      else if (gbGBCColorType == 2) // GBASP Hardware
      {
         gbPalette[0x20] = 0x9c00;
         gbPalette[0x21] = 0x6340;
         gbPalette[0x22] = 0x10c6;
         gbPalette[0x23] = 0xdb97;
         gbPalette[0x24] = 0x7622;
         gbPalette[0x25] = 0x3e57;
         gbPalette[0x26] = 0x2e12;
         gbPalette[0x27] = 0x95c3;
         gbPalette[0x28] = 0x1095;
         gbPalette[0x29] = 0x488c;
         gbPalette[0x2a] = 0x8241;
         gbPalette[0x2b] = 0xde8c;
         gbPalette[0x2c] = 0xfabc;
         gbPalette[0x2d] = 0x0e81;
         gbPalette[0x2e] = 0x7675;
         gbPalette[0x2f] = 0xfdec;
         gbPalette[0x30] = 0xddfd;
         gbPalette[0x31] = 0x5995;
         gbPalette[0x32] = 0x066a;
         gbPalette[0x33] = 0xed1e;
         gbPalette[0x34] = 0x1e84;
         gbPalette[0x35] = 0x1d14;
         gbPalette[0x36] = 0x11c3;
         gbPalette[0x37] = 0x2749;
         gbPalette[0x38] = 0xa727;
         gbPalette[0x39] = 0x6266;
         gbPalette[0x3a] = 0xe27b;
         gbPalette[0x3b] = 0xe3fc;
         gbPalette[0x3c] = 0x1f76;
         gbPalette[0x3d] = 0xf158;
         gbPalette[0x3e] = 0x468e;
         gbPalette[0x3f] = 0xa540;
      }

      // The CGB BIOS palette selection has to be done by VBA if BIOS is skipped.
      if (!(gbRom[0x143] & 0x80) && !inBios)
      {
         gbSelectColorizationPalette();
      }
   }
   else
   {
      for (int i = 0; i < 8; i++)
         gbPalette[i] = systemGbPalette[gbPaletteOption * 8 + i];
   }

   GBTIMER_MODE_0_CLOCK_TICKS = 256;
   GBTIMER_MODE_1_CLOCK_TICKS = 4;
   GBTIMER_MODE_2_CLOCK_TICKS = 16;
   GBTIMER_MODE_3_CLOCK_TICKS = 64;

   GBLY_INCREMENT_CLOCK_TICKS = 114;
   gbTimerTicks = GBTIMER_MODE_0_CLOCK_TICKS;
   gbTimerClockTicks = GBTIMER_MODE_0_CLOCK_TICKS;
   gbSerialTicks = 0;
   gbSerialBits = 0;
   gbSerialOn = 0;
   gbWindowLine = -1;
   gbTimerOn = 0;
   gbTimerMode = 0;
   gbSpeed = 0;
   gbJoymask[0] = gbJoymask[1] = gbJoymask[2] = gbJoymask[3] = 0;

   if (gbCgbMode)
   {
      gbSpeed = 0;
      gbHdmaOn = 0;
      gbHdmaSource = 0x99d0;
      gbHdmaDestination = 0x99d0;
      gbVramBank = 0;
      gbWramBank = 1;
   }

   // used to clean the borders
   if (gbSgbMode)
   {
      gbSgbResetFlag = true;
      gbSgbReset();
      if (gbBorderOn)
         gbSgbRenderBorder();
      gbSgbResetFlag = false;
   }

   for (int i = 0; i < 4; i++)
      gbBgp[i] = gbObp0[i] = gbObp1[i] = i;

   memset(&gbDataMBC1, 0, sizeof(gbDataMBC1));
   gbDataMBC1.mapperROMBank = 1;

   gbDataMBC2.mapperRAMEnable = 0;
   gbDataMBC2.mapperROMBank = 1;

   memset(&gbDataMBC3, 0, 6 * sizeof(int));
   gbDataMBC3.mapperROMBank = 1;

   memset(&gbDataMBC5, 0, sizeof(gbDataMBC5));
   gbDataMBC5.mapperROMBank = 1;
   gbDataMBC5.isRumbleCartridge = gbRumble;

   memset(&gbDataHuC1, 0, sizeof(gbDataHuC1));
   gbDataHuC1.mapperROMBank = 1;

   memset(&gbDataHuC3, 0, sizeof(gbDataHuC3));
   gbDataHuC3.mapperROMBank = 1;

   memset(&gbDataTAMA5, 0, 26 * sizeof(int));
   gbDataTAMA5.mapperROMBank = 1;

   memset(&gbDataMMM01, 0, sizeof(gbDataMMM01));
   gbDataMMM01.mapperROMBank = 1;

   if (inBios)
   {
      gbMemoryMap[GB_MEM_CART_BANK0] = &gbMemory[0x0000];
   }
   else
   {
      gbMemoryMap[GB_MEM_CART_BANK0] = &gbRom[0x0000];
   }

   gbMemoryMap[GB_MEM_CART_BANK0 + 1] = &gbRom[0x1000];
   gbMemoryMap[GB_MEM_CART_BANK0 + 2] = &gbRom[0x2000];
   gbMemoryMap[GB_MEM_CART_BANK0 + 3] = &gbRom[0x3000];
   gbMemoryMap[GB_MEM_CART_BANK1] = &gbRom[0x4000];
   gbMemoryMap[GB_MEM_CART_BANK1 + 1] = &gbRom[0x5000];
   gbMemoryMap[GB_MEM_CART_BANK1 + 2] = &gbRom[0x6000];
   gbMemoryMap[GB_MEM_CART_BANK1 + 3] = &gbRom[0x7000];
   gbMemoryMap[GB_MEM_VRAM] = &gbMemory[0x8000];
   gbMemoryMap[GB_MEM_VRAM + 1] = &gbMemory[0x9000];
   gbMemoryMap[GB_MEM_EXTERNAL_RAM] = &gbMemory[0xa000];
   gbMemoryMap[GB_MEM_EXTERNAL_RAM + 1] = &gbMemory[0xb000];
   gbMemoryMap[GB_MEM_WRAM_BANK0] = &gbMemory[0xc000];
   gbMemoryMap[GB_MEM_WRAM_BANK1] = &gbMemory[0xd000];
   gbMemoryMap[GB_MEM_BASE_E] = &gbMemory[0xe000];
   gbMemoryMap[GB_MEM_BASE_F] = &gbMemory[0xf000];

   if (gbCgbMode)
   {
      gbMemoryMap[GB_MEM_VRAM] = &gbVram[0x0000];
      gbMemoryMap[GB_MEM_VRAM + 1] = &gbVram[0x1000];
      gbMemoryMap[GB_MEM_WRAM_BANK0] = &gbWram[0x0000];
      gbMemoryMap[GB_MEM_WRAM_BANK1] = &gbWram[0x1000];
   }

   if (gbRam)
   {
      gbMemoryMap[GB_MEM_EXTERNAL_RAM] = &gbRam[0x0000];
      gbMemoryMap[GB_MEM_EXTERNAL_RAM + 1] = &gbRam[0x1000];
   }

   gbSoundReset();

   systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

   gbLastTime = systemGetClock();
   gbFrameCount = 0;

   gbScreenOn = true;
   gbSystemMessage = false;

   gbCheatWrite(true); // Emulates GS codes.
}

bool gbInit()
{
   gbGenFilter();
   gbSgbInit();
   setColorizerHack(false);

   gbMemory = (uint8_t*)malloc(65536);
   if (!gbMemory) return false;

   pix = (pixFormat*)calloc(1, 4 * 256 * 224);
   if (!pix) return false;

   gbLineBuffer = (uint16_t*)malloc(160 * sizeof(uint16_t));
   if (!gbLineBuffer) return false;

   return true;
}

bool gbReadGSASnapshot(const char* fileName)
{
   FILE* file = fopen(fileName, "rb");

   if (!file)
   {
      systemMessage(MSG_CANNOT_OPEN_FILE, N_("Cannot open file %s"), fileName);
      return false;
   }

   fseek(file, 0x4, SEEK_SET);
   char buffer[16];
   char buffer2[16];
   FREAD_UNCHECKED(buffer, 1, 15, file);
   buffer[15] = 0;
   memcpy(buffer2, &gbRom[0x134], 15);
   buffer2[15] = 0;
   if (memcmp(buffer, buffer2, 15))
   {
      systemMessage(MSG_CANNOT_IMPORT_SNAPSHOT_FOR,
          N_("Cannot import snapshot for %s. Current game is %s"),
          buffer,
          buffer2);
      fclose(file);
      return false;
   }
   fseek(file, 0x13, SEEK_SET);
   switch (gbRomType)
   {
   case 0x03:
   case 0x0f:
   case 0x10:
   case 0x13:
   case 0x1b:
   case 0x1e:
   case 0xff:
      FREAD_UNCHECKED(gbRam, 1, (gbRamSizeMask + 1), file);
      break;
   case 0x06:
   case 0x22:
      FREAD_UNCHECKED(&gbMemory[0xa000], 1, 256, file);
      break;
   default:
      systemMessage(MSG_UNSUPPORTED_SNAPSHOT_FILE,
          N_("Unsupported snapshot file %s"),
          fileName);
      fclose(file);
      return false;
   }
   fclose(file);
   gbReset();
   return true;
}

variable_desc gbSaveGameStruct[] = {
   { &PC.W, sizeof(uint16_t) },
   { &SP.W, sizeof(uint16_t) },
   { &AF.W, sizeof(uint16_t) },
   { &BC.W, sizeof(uint16_t) },
   { &DE.W, sizeof(uint16_t) },
   { &HL.W, sizeof(uint16_t) },
   { &IFF, sizeof(uint8_t) },
   { &GBLCD_MODE_0_CLOCK_TICKS, sizeof(int) },
   { &GBLCD_MODE_1_CLOCK_TICKS, sizeof(int) },
   { &GBLCD_MODE_2_CLOCK_TICKS, sizeof(int) },
   { &GBLCD_MODE_3_CLOCK_TICKS, sizeof(int) },
   { &GBDIV_CLOCK_TICKS, sizeof(int) },
   { &GBLY_INCREMENT_CLOCK_TICKS, sizeof(int) },
   { &GBTIMER_MODE_0_CLOCK_TICKS, sizeof(int) },
   { &GBTIMER_MODE_1_CLOCK_TICKS, sizeof(int) },
   { &GBTIMER_MODE_2_CLOCK_TICKS, sizeof(int) },
   { &GBTIMER_MODE_3_CLOCK_TICKS, sizeof(int) },
   { &GBSERIAL_CLOCK_TICKS, sizeof(int) },
   { &GBSYNCHRONIZE_CLOCK_TICKS, sizeof(int) },
   { &gbDivTicks, sizeof(int) },
   { &gbLcdMode, sizeof(int) },
   { &gbLcdTicks, sizeof(int) },
   { &gbLcdLYIncrementTicks, sizeof(int) },
   { &gbTimerTicks, sizeof(int) },
   { &gbTimerClockTicks, sizeof(int) },
   { &gbSerialTicks, sizeof(int) },
   { &gbSerialBits, sizeof(int) },
   { &gbInt48Signal, sizeof(int) },
   { &gbInterruptWait, sizeof(int) },
   { &gbSynchronizeTicks, sizeof(int) },
   { &gbTimerOn, sizeof(int) },
   { &gbTimerMode, sizeof(int) },
   { &gbSerialOn, sizeof(int) },
   { &gbWindowLine, sizeof(int) },
   { &gbCgbMode, sizeof(int) },
   { &gbVramBank, sizeof(int) },
   { &gbWramBank, sizeof(int) },
   { &gbHdmaSource, sizeof(int) },
   { &gbHdmaDestination, sizeof(int) },
   { &gbHdmaBytes, sizeof(int) },
   { &gbHdmaOn, sizeof(int) },
   { &gbSpeed, sizeof(int) },
   { &gbSgbMode, sizeof(int) },
   { &register_IF, sizeof(uint8_t) },
   { &register_LCDC, sizeof(uint8_t) },
   { &register_STAT, sizeof(uint8_t) },
   { &register_LY, sizeof(uint8_t) },
   { &register_IE, sizeof(uint8_t) },
   { &gbBgp[0], sizeof(uint8_t) },
   { &gbBgp[1], sizeof(uint8_t) },
   { &gbBgp[2], sizeof(uint8_t) },
   { &gbBgp[3], sizeof(uint8_t) },
   { &gbObp0[0], sizeof(uint8_t) },
   { &gbObp0[1], sizeof(uint8_t) },
   { &gbObp0[2], sizeof(uint8_t) },
   { &gbObp0[3], sizeof(uint8_t) },
   { &gbObp1[0], sizeof(uint8_t) },
   { &gbObp1[1], sizeof(uint8_t) },
   { &gbObp1[2], sizeof(uint8_t) },
   { &gbObp1[3], sizeof(uint8_t) },
   { NULL, 0 }
};

bool gbWritePNGFile(const char* fileName)
{
   //if (gbBorderOn)
   //return utilWritePNGFile(fileName, 256, 224, pix);
   //return utilWritePNGFile(fileName, 160, 144, pix);
   return false;
}

bool gbWriteBMPFile(const char* fileName)
{
   //if (gbBorderOn)
   //return utilWriteBMPFile(fileName, 256, 224, pix);
   //return utilWriteBMPFile(fileName, 160, 144, pix);
   return false;
}

void gbCleanUp()
{
   if (gbRam != NULL)
   {
      free(gbRam);
      gbRam = NULL;
   }

   if (gbRom != NULL)
   {
      free(gbRom);
      gbRom = NULL;
   }

   if (bios != NULL)
   {
      free(bios);
      bios = NULL;
   }

   if (gbMemory != NULL)
   {
      free(gbMemory);
      gbMemory = NULL;
   }

   if (gbLineBuffer != NULL)
   {
      free(gbLineBuffer);
      gbLineBuffer = NULL;
   }

   if (pix != NULL)
   {
      free(pix);
      pix = NULL;
   }

   gbSgbShutdown();

   if (gbVram != NULL)
   {
      free(gbVram);
      gbVram = NULL;
   }

   if (gbWram != NULL)
   {
      free(gbWram);
      gbWram = NULL;
   }

   if (gbTAMA5ram != NULL)
   {
      free(gbTAMA5ram);
      gbTAMA5ram = NULL;
   }

   systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
}

bool gbLoadRom(const char* szFile)
{
   int size = 0;

   if (gbRom != NULL)
   {
      gbCleanUp();
   }

   systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

   gbRom = utilLoad(szFile,
       utilIsGBImage,
       NULL,
       size);
   if (!gbRom)
      return false;

   gbRomSize = size;

   gbBatteryError = false;

   if (bios != NULL)
   {
      free(bios);
      bios = NULL;
   }
   bios = (uint8_t*)calloc(1, 0x900);

   return gbUpdateSizes();
}

int gbGetNextEvent(int _clockTicks)
{
   if (register_LCDC & 0x80)
   {
      if (gbLcdTicks < _clockTicks)
         _clockTicks = gbLcdTicks;

      if (gbLcdTicksDelayed < _clockTicks)
         _clockTicks = gbLcdTicksDelayed;

      if (gbLcdLYIncrementTicksDelayed < _clockTicks)
         _clockTicks = gbLcdLYIncrementTicksDelayed;
   }

   if (gbLcdLYIncrementTicks < _clockTicks)
      _clockTicks = gbLcdLYIncrementTicks;

   if (gbSerialOn && (gbSerialTicks < _clockTicks))
      _clockTicks = gbSerialTicks;

   if (gbTimerOn && (((gbInternalTimer)&gbTimerMask[gbTimerMode]) + 1 < _clockTicks))
      _clockTicks = ((gbInternalTimer)&gbTimerMask[gbTimerMode]) + 1;

   //if(soundTicks && (soundTicks < _clockTicks))
   //  _clockTicks = soundTicks;

   if ((_clockTicks <= 0) || (gbInterruptWait))
      _clockTicks = 1;

   return _clockTicks;
}

void gbDrawLine()
{
   pixFormat* dest = pix + gbBorderLineSkip * (register_LY + gbBorderRowSkip)
       + gbBorderColumnSkip;
   for (int x = 0; x < 160;)
   {
      *dest++ = systemColorMap16[gbLineMix[x++]];
      *dest++ = systemColorMap16[gbLineMix[x++]];
      *dest++ = systemColorMap16[gbLineMix[x++]];
      *dest++ = systemColorMap16[gbLineMix[x++]];

      *dest++ = systemColorMap16[gbLineMix[x++]];
      *dest++ = systemColorMap16[gbLineMix[x++]];
      *dest++ = systemColorMap16[gbLineMix[x++]];
      *dest++ = systemColorMap16[gbLineMix[x++]];

      *dest++ = systemColorMap16[gbLineMix[x++]];
      *dest++ = systemColorMap16[gbLineMix[x++]];
      *dest++ = systemColorMap16[gbLineMix[x++]];
      *dest++ = systemColorMap16[gbLineMix[x++]];

      *dest++ = systemColorMap16[gbLineMix[x++]];
      *dest++ = systemColorMap16[gbLineMix[x++]];
      *dest++ = systemColorMap16[gbLineMix[x++]];
      *dest++ = systemColorMap16[gbLineMix[x++]];
   }
   if (gbBorderOn)
      dest += gbBorderColumnSkip;
}

static void gbUpdateJoypads(bool readSensors)
{
   if (systemReadJoypads())
   {
      // read joystick
      if (gbSgbMode && gbSgbMultiplayer)
      {
         if (gbSgbFourPlayers)
         {
            gbJoymask[0] = systemReadJoypad(0);
            gbJoymask[1] = systemReadJoypad(1);
            gbJoymask[2] = systemReadJoypad(2);
            gbJoymask[3] = systemReadJoypad(3);
         }
         else
         {
            gbJoymask[0] = systemReadJoypad(0);
            gbJoymask[1] = systemReadJoypad(1);
         }
      }
      else
      {
         gbJoymask[0] = systemReadJoypad(-1);
      }
   }

   if (readSensors && gbRomType == 0x22)
   {
      systemUpdateMotionSensor();
   }
}

void gbEmulate(int ticksToStop)
{
   gbRegister tempRegister;
   uint8_t tempValue;
   int8_t offset;

   clockTicks = 0;
   gbDmaTicks = 0;

   int opcode = 0;

   int opcode1 = 0;
   int opcode2 = 0;
   bool execute = false;
   bool frameDone = false;

   gbUpdateJoypads(true);

   while (1)
   {
      uint16_t oldPCW = PC.W;

      if (IFF & 0x80)
      {
         if (register_LCDC & 0x80)
         {
            clockTicks = gbLcdTicks;
         }
         else
            clockTicks = 1000;

         clockTicks = gbGetNextEvent(clockTicks);
      }
      else
      {
         // First we apply the clockTicks, then we execute the opcodes.
         opcode1 = 0;
         opcode2 = 0;
         execute = true;

         opcode2 = opcode1 = opcode = gbReadMemory(PC.W++);

         // If HALT state was launched while IME = 0 and (register_IF & register_IE & 0x1F),
         // PC.W is not incremented for the first byte of the next instruction.
         if (IFF & 2)
         {
            PC.W--;
            IFF &= ~2;
         }

         clockTicks = gbCycles[opcode];

         switch (opcode)
         {
         case 0xCB:
            // extended opcode
            opcode2 = opcode = gbReadMemory(PC.W++);
            clockTicks = gbCyclesCB[opcode];
            break;
         }
         gbOldClockTicks = clockTicks - 1;
         gbIntBreak = 1;
      }

      if (!emulating)
         return;

      // For 'breakpoint' support (opcode 0xFC is considered as a breakpoint)
      if ((clockTicks == 0) && execute)
      {
         PC.W = oldPCW;
         return;
      }

      if (!(IFF & 0x80))
         clockTicks = 1;

   gbRedoLoop:

      if (gbInterruptWait)
         gbInterruptWait = 0;
      else
         gbInterruptLaunched = 0;

      // Used for the EI/DI instruction's delay.
      if (IFF & 0x38)
      {
         int tempIFF = (IFF >> 4) & 3;

         if (tempIFF <= clockTicks)
         {
            tempIFF = 0;
            IFF |= 1;
         }
         else
            tempIFF -= clockTicks;
         IFF = (IFF & 0xCF) | (tempIFF << 4);

         if (IFF & 0x08)
            IFF &= 0x82;
      }

      if (register_LCDCBusy)
      {
         register_LCDCBusy -= clockTicks;
         if (register_LCDCBusy < 0)
            register_LCDCBusy = 0;
      }

      if (gbSgbMode)
      {
         if (gbSgbPacketTimeout)
         {
            gbSgbPacketTimeout -= clockTicks;

            if (gbSgbPacketTimeout <= 0)
               gbSgbResetPacketState();
         }
      }

      ticksToStop -= clockTicks;

      gbUpdateSoundTicks(clockTicks << (1 - gbSpeed));

      // DIV register emulation
      gbDivTicks -= clockTicks;
      while (gbDivTicks <= 0)
      {
         gbMemory[REG_DIV]++;
         gbDivTicks += GBDIV_CLOCK_TICKS;
      }

      if (register_LCDC & 0x80)
      {
         // LCD stuff

         gbLcdTicks -= clockTicks;
         gbLcdTicksDelayed -= clockTicks;
         gbLcdLYIncrementTicks -= clockTicks;
         gbLcdLYIncrementTicksDelayed -= clockTicks;

         // our counters are off, see what we need to do

         // This looks (and kinda is) complicated, however this
         // is the only way I found to emulate properly the way
         // the real hardware operates...
         while (((gbLcdTicks <= 0) && (gbLCDChangeHappened == false)) || ((gbLcdTicksDelayed <= 0) && (gbLCDChangeHappened == true)) || ((gbLcdLYIncrementTicks <= 0) && (gbLYChangeHappened == false)) || ((gbLcdLYIncrementTicksDelayed <= 0) && (gbLYChangeHappened == true)))
         {

            if ((gbLcdLYIncrementTicks <= 0) && (!gbLYChangeHappened))
            {
               gbLYChangeHappened = true;
               gbMemory[REG_LY] = register_LY = (register_LY + 1) % 154;

               if (register_LY == 0x91)
               {
                  /* if (IFF & 0x80)
              gbScreenOn = !gbScreenOn;
            else*/
                  if (register_LCDC & 0x80)
                     gbScreenOn = true;
               }

               gbLcdLYIncrementTicks += GBLY_INCREMENT_CLOCK_TICKS;

               if (gbLcdMode == 1)
               {

                  if (register_LY == 153)
                     gbLcdLYIncrementTicks -= GBLY_INCREMENT_CLOCK_TICKS - gbLine99Ticks;
                  else if (register_LY == 0)
                     gbLcdLYIncrementTicks += GBLY_INCREMENT_CLOCK_TICKS - gbLine99Ticks;
               }

               // GB only 'bug' : Halt state is broken one tick before LY==LYC interrupt
               // is reflected in the registers.
               if ((gbHardware & 5) && (IFF & 0x80) && (register_LY == gbMemory[REG_LYC])
                   && (register_STAT & 0x40) && (register_LY != 0))
               {
                  if (!((gbLcdModeDelayed != 1) && (register_LY == 0)))
                  {
                     gbInt48Signal &= ~9;
                     gbCompareLYToLYC();
                     gbLYChangeHappened = false;
                     gbMemory[REG_STAT] = register_STAT;
                     gbMemory[IF_FLAG] = register_IF;
                  }

                  gbLcdLYIncrementTicksDelayed += GBLY_INCREMENT_CLOCK_TICKS - gbLine99Ticks + 1;
               }
            }

            if ((gbLcdTicks <= 0) && (!gbLCDChangeHappened))
            {
               gbLCDChangeHappened = true;

               switch (gbLcdMode)
               {
               case 0:
               {
                  // H-Blank
                  // check if we reached the V-Blank period
                  if (register_LY == 144)
                  {
                     // Yes, V-Blank
                     // set the LY increment counter
                     if (gbHardware & 0x5)
                     {
                        register_IF |= 1; // V-Blank interrupt
                     }

                     gbInt48Signal &= ~6;
                     if (register_STAT & 0x10)
                     {
                        // send LCD interrupt only if no interrupt 48h signal...
                        if ((!(gbInt48Signal & 1)) && ((!(gbInt48Signal & 8)) || (gbHardware & 0x0a)))
                        {
                           register_IF |= 2;
                           gbInterruptLaunched |= 2;
                           if (gbHardware & 0xa)
                              gbInterruptWait = 1;
                        }
                        gbInt48Signal |= 2;
                     }
                     gbInt48Signal &= ~1;

                     gbLcdTicks += GBLCD_MODE_1_CLOCK_TICKS;
                     gbLcdMode = 1;
                  }
                  else
                  {
                     // go the the OAM being accessed mode
                     gbLcdTicks += GBLCD_MODE_2_CLOCK_TICKS;
                     gbLcdMode = 2;

                     gbInt48Signal &= ~6;
                     if (register_STAT & 0x20)
                     {
                        // send LCD interrupt only if no interrupt 48h signal...
                        if (!gbInt48Signal)
                        {
                           register_IF |= 2;
                           gbInterruptLaunched |= 2;
                        }
                        gbInt48Signal |= 4;
                     }
                     gbInt48Signal &= ~1;
                  }
               }
               break;
               case 1:
               {
                  // V-Blank
                  // next mode is OAM being accessed mode
                  gbInt48Signal &= ~5;
                  if (register_STAT & 0x20)
                  {
                     // send LCD interrupt only if no interrupt 48h signal...
                     if (!gbInt48Signal)
                     {
                        register_IF |= 2;
                        gbInterruptLaunched |= 2;
                        if ((gbHardware & 0xa) && (IFF & 0x80))
                           gbInterruptWait = 1;
                     }
                     gbInt48Signal |= 4;
                  }
                  gbInt48Signal &= ~2;

                  gbLcdTicks += GBLCD_MODE_2_CLOCK_TICKS;

                  gbLcdMode = 2;
                  register_LY = 0x00;
               }
               break;
               case 2:
               {

                  // OAM being accessed mode
                  // next mode is OAM and VRAM in use
                  if ((gbScreenOn) && (register_LCDC & 0x80))
                  {
                     gbRenderLine(false);
                     // Used to add a one tick delay when a window line is drawn.
                     //(fixes a part of Carmaggedon problem)
                     if ((register_LCDC & 0x01 || gbCgbMode) && (register_LCDC & 0x20) && (gbWindowLine != -2))
                     {

                        int tempinUseRegister_WY = inUseRegister_WY;
                        int tempgbWindowLine = gbWindowLine;

                        if ((tempgbWindowLine == -1) || (tempgbWindowLine > 144))
                        {
                           tempinUseRegister_WY = oldRegister_WY;
                           if (register_LY > oldRegister_WY)
                              tempgbWindowLine = 146;
                        }

                        if (register_LY >= tempinUseRegister_WY)
                        {

                           if (tempgbWindowLine == -1)
                              tempgbWindowLine = 0;

                           int wx = gbMemory[REG_WX];
                           wx -= 7;
                           if (wx < 0)
                              wx = 0;

                           if ((wx <= 159) && (tempgbWindowLine <= 143))
                           {
                              for (int i = wx; i < 300; i++)
                                 if (gbSpeed)
                                    gbSpritesTicks[i] += 3;
                                 else
                                    gbSpritesTicks[i] += 1;
                           }
                        }
                     }
                  }

                  gbInt48Signal &= ~7;

                  gbLcdTicks += GBLCD_MODE_3_CLOCK_TICKS + gbSpritesTicks[299];
                  gbLcdMode = 3;
               }
               break;
               case 3:
               {
                  // OAM and VRAM in use
                  // next mode is H-Blank

                  gbInt48Signal &= ~7;
                  if (register_STAT & 0x08)
                  {
                     // send LCD interrupt only if no interrupt 48h signal...
                     if (!(gbInt48Signal & 8))
                     {
                        register_IF |= 2;
                        if ((gbHardware & 0xa) && (IFF & 0x80))
                           gbInterruptWait = 1;
                     }
                     gbInt48Signal |= 1;
                  }

                  gbLcdTicks += GBLCD_MODE_0_CLOCK_TICKS - gbSpritesTicks[299];

                  gbLcdMode = 0;

                  // No HDMA during HALT !
                  if (gbHdmaOn && (!(IFF & 0x80) || (register_IE & register_IF & 0x1f)))
                  {
                     gbDoHdma();
                  }
               }
               break;
               }
            }

            if ((gbLcdTicksDelayed <= 0) && (gbLCDChangeHappened))
            {
               int framesToSkip = systemFrameSkip;

               if ((gbJoymask[0] >> 10) & 1)
                  framesToSkip = 9;

               //gbLcdTicksDelayed = gbLcdTicks+1;
               gbLCDChangeHappened = false;
               switch (gbLcdModeDelayed)
               {
               case 0:
               {
                  // H-Blank

                  memset(gbSCYLine, gbSCYLine[299], sizeof(gbSCYLine));
                  memset(gbSCXLine, gbSCXLine[299], sizeof(gbSCXLine));
                  memset(gbBgpLine, gbBgpLine[299], sizeof(gbBgpLine));
                  memset(gbObp0Line, gbObp0Line[299], sizeof(gbObp0Line));
                  memset(gbObp1Line, gbObp1Line[299], sizeof(gbObp1Line));
                  memset(gbSpritesTicks, gbSpritesTicks[299], sizeof(gbSpritesTicks));

                  if (gbWindowLine < 0)
                     oldRegister_WY = gbMemory[REG_WY];
                  // check if we reached the V-Blank period
                  if (register_LY == 144)
                  {
                     // Yes, V-Blank
                     // set the LY increment counter

                     if (register_LCDC & 0x80)
                     {
                        if (gbHardware & 0xa)
                        {

                           register_IF |= 1; // V-Blank interrupt
                           gbInterruptLaunched |= 1;
                        }
                     }

                     gbLcdTicksDelayed += GBLCD_MODE_1_CLOCK_TICKS;
                     gbLcdModeDelayed = 1;

                     gbFrameCount++;
                     systemFrame();

                     if ((gbFrameCount % 10) == 0)
                        system10Frames(60);

                     if (gbFrameCount >= 60)
                     {
                        uint32_t currentTime = systemGetClock();
                        if (currentTime != gbLastTime)
                           systemShowSpeed(100000 / (currentTime - gbLastTime));
                        else
                           systemShowSpeed(0);
                        gbLastTime = currentTime;
                        gbFrameCount = 0;
                     }

                     int newmask = gbJoymask[0] & 255;

                     if (newmask)
                     {
                        gbMemory[IF_FLAG] = register_IF |= 16;
                     }

                     newmask = (gbJoymask[0] >> 10);

                     speedup = false;

                     if (newmask & 1)
                        speedup = true;

                     gbCapture = (newmask & 2) ? true : false;

                     if (gbCapture && !gbCapturePrevious)
                     {
                        gbCaptureNumber++;
                        systemScreenCapture(gbCaptureNumber);
                     }
                     gbCapturePrevious = gbCapture;

                     if (gbFrameSkipCount >= framesToSkip)
                     {

                        if (!gbSgbMask)
                        {
                           if (gbBorderOn)
                              gbSgbRenderBorder();
                           //if (gbScreenOn)
                           systemDrawScreen(pix);
                           if (systemPauseOnFrame())
                              ticksToStop = 0;
                        }
                        gbFrameSkipCount = 0;
                     }
                     else
                        gbFrameSkipCount++;

                     frameDone = true;
                  }
                  else
                  {
                     // go the the OAM being accessed mode
                     gbLcdTicksDelayed += GBLCD_MODE_2_CLOCK_TICKS;
                     gbLcdModeDelayed = 2;
                     gbInt48Signal &= ~3;
                  }
               }
               break;
               case 1:
               {
                  // V-Blank
                  // next mode is OAM being accessed mode

                  // gbScreenOn = true;

                  oldRegister_WY = gbMemory[REG_WY];

                  gbLcdTicksDelayed += GBLCD_MODE_2_CLOCK_TICKS;
                  gbLcdModeDelayed = 2;

                  // reset the window line
                  gbWindowLine = -1;
               }
               break;
               case 2:
               {
                  // OAM being accessed mode
                  // next mode is OAM and VRAM in use
                  gbLcdTicksDelayed += GBLCD_MODE_3_CLOCK_TICKS + gbSpritesTicks[299];
                  gbLcdModeDelayed = 3;
               }
               break;
               case 3:
               {
                  // OAM and VRAM in use
                  // next mode is H-Blank
                  if ((register_LY < 144) && (register_LCDC & 0x80) && gbScreenOn)
                  {
                     if (!gbSgbMask)
                     {
                        if (gbFrameSkipCount >= framesToSkip)
                        {
                           if (!gbBlackScreen)
                           {
                              gbRenderLine(true);
                           }
                           else if (gbBlackScreen)
                           {
                              uint16_t color = gbColorOption ? gbColorFilter[0] : 0;
                              if (!gbCgbMode)
                                 color = gbColorOption ? gbColorFilter[gbPalette[3] & 0x7FFF] : gbPalette[3] & 0x7FFF;
                              for (int i = 0; i < 160; i++)
                              {
                                 gbLineMix[i] = color;
                                 gbLineBuffer[i] = 0;
                              }
                           }
                           gbDrawLine();
                        }
                     }
                  }
                  gbLcdTicksDelayed += GBLCD_MODE_0_CLOCK_TICKS - gbSpritesTicks[299];
                  gbLcdModeDelayed = 0;
               }
               break;
               }
            }

            if ((gbLcdLYIncrementTicksDelayed <= 0) && (gbLYChangeHappened == true))
            {

               gbLYChangeHappened = false;

               if (!((gbLcdMode != 1) && (register_LY == 0)))
               {
                  {
                     gbInt48Signal &= ~8;
                     gbCompareLYToLYC();
                     if ((gbInt48Signal == 8) && (!((register_LY == 0) && (gbHardware & 1))))
                        gbInterruptLaunched |= 2;
                     if ((gbHardware & (gbSpeed ? 8 : 2)) && (register_LY == 0) && ((register_STAT & 0x44) == 0x44) && (gbLcdLYIncrementTicksDelayed == 0))
                     {
                        gbInterruptWait = 1;
                     }
                  }
               }
               gbLcdLYIncrementTicksDelayed += GBLY_INCREMENT_CLOCK_TICKS;

               if (gbLcdModeDelayed == 1)
               {

                  if (register_LY == 153)
                     gbLcdLYIncrementTicksDelayed -= GBLY_INCREMENT_CLOCK_TICKS - gbLine99Ticks;
                  else if (register_LY == 0)
                     gbLcdLYIncrementTicksDelayed += GBLY_INCREMENT_CLOCK_TICKS - gbLine99Ticks;
               }
               gbMemory[IF_FLAG] = register_IF;
               gbMemory[REG_STAT] = register_STAT;
            }
         }
         gbMemory[IF_FLAG] = register_IF;
         gbMemory[REG_STAT] = register_STAT = (register_STAT & 0xfc) | gbLcdModeDelayed;
      }
      else
      {

         // Used to update the screen with white lines when it's off.
         // (it looks strange, but it's kinda accurate :p)
         // You can try the Mario Demo Vx.x for exemple
         // (check the bottom 2 lines while moving)
         if (!gbWhiteScreen)
         {
            gbScreenTicks -= clockTicks;
            gbLcdLYIncrementTicks -= clockTicks;
            while (gbLcdLYIncrementTicks <= 0)
            {
               register_LY = ((register_LY + 1) % 154);
               gbLcdLYIncrementTicks += GBLY_INCREMENT_CLOCK_TICKS;
            }
            if (gbScreenTicks <= 0)
            {
               gbWhiteScreen = 1;
               uint8_t register_LYLcdOff = ((register_LY + 154) % 154);
               for (register_LY = 0; register_LY <= 0x90; register_LY++)
               {
                  uint16_t color = gbColorOption ? gbColorFilter[0x7FFF] : 0x7FFF;
                  if (!gbCgbMode)
                     color = gbColorOption ? gbColorFilter[gbPalette[0] & 0x7FFF] : gbPalette[0] & 0x7FFF;
                  for (int i = 0; i < 160; i++)
                  {
                     gbLineMix[i] = color;
                     gbLineBuffer[i] = 0;
                  }
                  gbDrawLine();
               }
               register_LY = register_LYLcdOff;
            }
         }

         if (gbWhiteScreen)
         {
            gbLcdLYIncrementTicks -= clockTicks;

            while (gbLcdLYIncrementTicks <= 0)
            {
               register_LY = ((register_LY + 1) % 154);
               gbLcdLYIncrementTicks += GBLY_INCREMENT_CLOCK_TICKS;
               if (register_LY < 144)
               {

                  uint16_t color = gbColorOption ? gbColorFilter[0x7FFF] : 0x7FFF;
                  if (!gbCgbMode)
                     color = gbColorOption ? gbColorFilter[gbPalette[0] & 0x7FFF] : gbPalette[0] & 0x7FFF;
                  for (int i = 0; i < 160; i++)
                  {
                     gbLineMix[i] = color;
                     gbLineBuffer[i] = 0;
                  }
                  gbDrawLine();
               }
               else if ((register_LY == 144) && (!systemFrameSkip))
               {
                  int framesToSkip = systemFrameSkip;
                  if (speedup)
                     framesToSkip = 9; // try 6 FPS during speedup
                  if ((gbFrameSkipCount >= framesToSkip) || (gbWhiteScreen == 1))
                  {
                     gbWhiteScreen = 2;

                     if (!gbSgbMask)
                     {
                        if (gbBorderOn)
                           gbSgbRenderBorder();
                        //if (gbScreenOn)
                        systemDrawScreen(pix);
                        if (systemPauseOnFrame())
                           ticksToStop = 0;
                     }
                  }

                  gbFrameCount++;

                  systemFrame();

                  if ((gbFrameCount % 10) == 0)
                     system10Frames(60);

                  if (gbFrameCount >= 60)
                  {
                     uint32_t currentTime = systemGetClock();
                     if (currentTime != gbLastTime)
                        systemShowSpeed(100000 / (currentTime - gbLastTime));
                     else
                        systemShowSpeed(0);
                     gbLastTime = currentTime;
                     gbFrameCount = 0;
                  }

                  frameDone = true;
               }
            }
         }
      }

      gbMemory[REG_STAT] = register_STAT;

#ifndef NO_LINK
      // serial emulation
      gbSerialOn = (gbMemory[REG_SC] & 0x80);
      static int SIOctr = 0;
      SIOctr++;
      if (SIOctr % 5)
         //Transfer Started
         if (gbSerialOn && (GetLinkMode() == LINK_GAMEBOY_IPC || GetLinkMode() == LINK_GAMEBOY_SOCKET))
         {
#ifdef OLD_GB_LINK
            if (linkConnected)
            {
               gbSerialTicks -= clockTicks;

               while (gbSerialTicks <= 0)
               {
                  // increment number of shifted bits
                  gbSerialBits++;
                  linkProc();
                  if (gbSerialOn && (gbMemory[REG_SC] & 1))
                  {
                     if (gbSerialBits == 8)
                     {
                        gbSerialBits = 0;
                        gbMemory[REG_SB] = 0xff;
                        gbMemory[REG_SC] &= 0x7f;
                        gbSerialOn = 0;
                        gbMemory[IF_FLAG] = register_IF |= 8;
                        gbSerialTicks = 0;
                     }
                  }
                  gbSerialTicks += GBSERIAL_CLOCK_TICKS;
               }
            }
            else
            {
#endif
               if (gbMemory[REG_SC] & 1)
               { //internal clocks (master)
                  gbSerialTicks -= clockTicks;

                  // overflow
                  while (gbSerialTicks <= 0)
                  {
                     // shift serial byte to right and put a 1 bit in its place
                     //      gbMemory[REG_SB] = 0x80 | (gbMemory[REG_SB]>>1);
                     // increment number of shifted bits
                     gbSerialBits++;
                     if (gbSerialBits >= 8)
                     {
                        // end of transmission
                        gbSerialTicks = 0;
                        gbSerialBits = 0;
                     }
                     else
                        gbSerialTicks += GBSERIAL_CLOCK_TICKS;
                  }
               }
               else //external clocks (slave)
               {
                  gbSerialTicks -= clockTicks;

                  // overflow
                  while (gbSerialTicks <= 0)
                  {
                     // shift serial byte to right and put a 1 bit in its place
                     //      gbMemory[REG_SB] = 0x80 | (gbMemory[REG_SB]>>1);
                     // increment number of shifted bits
                     gbSerialBits++;
                     if (gbSerialBits >= 8)
                     {
                        // end of transmission
                        uint16_t dat = 0;
                        if (LinkIsWaiting)
                           if (gbSerialFunction)
                           { // external device
                              gbSIO_SC = gbMemory[REG_SC];
                              if (!LinkFirstTime)
                              {
                                 dat = (gbSerialFunction(gbMemory[REG_SB]) << 8) | 1;
                              }
                              else //external clock not suppose to start a transfer, but there are time where both side using external clock and couldn't communicate properly
                              {
                                 if (gbMemory)
                                    gbSerialOn = (gbMemory[REG_SC] & 0x80);
                                 dat = gbLinkUpdate(gbMemory[REG_SB], gbSerialOn);
                              }
                              gbMemory[REG_SB] = (dat >> 8);
                           } //else
                        gbSerialTicks = 0;
                        if ((dat & 1) && (gbMemory[REG_SC] & 0x80)) //(dat & 1)==1 when reply data received
                        {
                           gbMemory[REG_SC] &= 0x7f;
                           gbSerialOn = 0;
                           gbMemory[IF_FLAG] = register_IF |= 8;
                        }
                        gbSerialBits = 0;
                     }
                     else
                        gbSerialTicks += GBSERIAL_CLOCK_TICKS;
                  }
               }
#ifdef OLD_GB_LINK
            }
#endif
         }
#endif // NO_LINK

      // timer emulation

      if (gbTimerOn)
      {
         gbTimerTicks = ((gbInternalTimer)&gbTimerMask[gbTimerMode]) + 1 - clockTicks;

         while (gbTimerTicks <= 0)
         {
            gbMemory[REG_TIMA]++;
            // timer overflow!
            if ((gbMemory[REG_TIMA] & 0xff) == 0)
            {
               // reload timer modulo
               gbMemory[REG_TIMA] = gbMemory[REG_TMA];
               // flag interrupt
               gbMemory[IF_FLAG] = register_IF |= 4;
            }
            gbTimerTicks += gbTimerClockTicks;
         }
         gbTimerOnChange = false;
         gbTimerModeChange = false;
      }

      gbInternalTimer -= clockTicks;
      while (gbInternalTimer < 0)
         gbInternalTimer += 0x100;

      clockTicks = 0;

      if (gbIntBreak == 1)
      {
         gbIntBreak = 0;
         if ((register_IE & register_IF & gbInterruptLaunched & 0x3) && ((IFF & 0x81) == 1) && (!gbInterruptWait) && (execute))
         {
            gbIntBreak = 2;
            PC.W = oldPCW;
            execute = false;
            gbOldClockTicks = 0;
         }
         if (gbOldClockTicks)
         {
            clockTicks = gbOldClockTicks;
            gbOldClockTicks = 0;
            goto gbRedoLoop;
         }
      }

      // Executes the opcode(s), and apply the instruction's remaining clockTicks (if any).
      if (execute)
      {
         switch (opcode1)
         {
         case 0xCB:
            // extended opcode
            switch (opcode2)
            {
#include "gbCodesCB.h"
            }
            break;
#include "gbCodes.h"
         }
         execute = false;

         if (clockTicks)
         {
            gbDmaTicks += clockTicks;
            clockTicks = 0;
         }
      }

      if (gbDmaTicks)
      {
         clockTicks = gbGetNextEvent(gbDmaTicks);

         if (clockTicks <= gbDmaTicks)
            gbDmaTicks -= clockTicks;
         else
         {
            clockTicks = gbDmaTicks;
            gbDmaTicks = 0;
         }

         goto gbRedoLoop;
      }

      // Remove the 'if an IE is pending' flag if IE has finished being executed.
      if ((IFF & 0x40) && !(IFF & 0x30))
         IFF &= 0x81;

      if ((register_IE & register_IF & 0x1f) && (IFF & 0x81) && (!gbInterruptWait))
      {

         if (IFF & 1)
         {
            // Add 5 ticks for the interrupt execution time
            gbDmaTicks += 5;

            if (gbIntBreak == 2)
            {
               gbDmaTicks--;
               gbIntBreak = 0;
            }

            if (register_IF & register_IE & 1)
               gbVblank_interrupt();
            else if (register_IF & register_IE & 2)
               gbLcd_interrupt();
            else if (register_IF & register_IE & 4)
               gbTimer_interrupt();
            else if (register_IF & register_IE & 8)
               gbSerial_interrupt();
            else if (register_IF & register_IE & 16)
               gbJoypad_interrupt();
         }

         IFF &= ~0x81;
      }

      if (IFF & 0x08)
         IFF &= ~0x79;

      // Used to apply the interrupt's execution time.
      if (gbDmaTicks)
      {
         clockTicks = gbGetNextEvent(gbDmaTicks);

         if (clockTicks <= gbDmaTicks)
            gbDmaTicks -= clockTicks;
         else
         {
            clockTicks = gbDmaTicks;
            gbDmaTicks = 0;
         }
         goto gbRedoLoop;
      }

      gbBlackScreen = false;

      if (ticksToStop <= 0 || frameDone)
      { // Stop loop
         return;
      }
   }
}

bool gbLoadRomData(const char* data, unsigned size)
{
   gbRomSize = size;
   if (gbRom != NULL)
   {
      gbCleanUp();
   }

   systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

   gbRom = (uint8_t*)calloc(1, gbRomSize);
   if (gbRom == NULL)
   {
      return 0;
   }

   memcpy(gbRom, data, gbRomSize);

   gbBatteryError = false;

   if (bios != NULL)
   {
      free(bios);
      bios = NULL;
   }
   bios = (uint8_t*)calloc(1, 0x900);
   return gbUpdateSizes();
}

#include <stddef.h>

unsigned int gbWriteSaveState(uint8_t* data, unsigned)
{
   uint8_t* orig = data;
   utilWriteIntMem(data, GBSAVE_GAME_VERSION);
   utilWriteMem(data, &gbRom[0x134], 15);
   utilWriteIntMem(data, useBios);
   utilWriteIntMem(data, inBios);
   utilWriteDataMem(data, gbSaveGameStruct);
   utilWriteMem(data, &IFF, 2);

   if (gbSgbMode)
      gbSgbSaveGame(data);

   gbMbcSaveGame(data);

   utilWriteMem(data, gbPalette, 128 * sizeof(uint16_t));
   utilWriteMem(data, &gbMemory[0x8000], 0x8000);

   if (gbRamSize && gbRam)
   {
      utilWriteIntMem(data, gbRamSize);
      utilWriteMem(data, gbRam, gbRamSize);
   }

   if (gbCgbMode)
   {
      utilWriteMem(data, gbVram, 0x4000);
      utilWriteMem(data, gbWram, 0x8000);
   }

   gbSoundSaveGame(data);

   // We dont care about cheat saves
   // gbCheatsSaveGame(data);

   utilWriteIntMem(data, gbLcdModeDelayed);
   utilWriteIntMem(data, gbLcdTicksDelayed);
   utilWriteIntMem(data, gbLcdLYIncrementTicksDelayed);
   utilWriteIntMem(data, gbSpritesTicks[299]);
   utilWriteIntMem(data, gbTimerModeChange);
   utilWriteIntMem(data, gbTimerOnChange);
   utilWriteIntMem(data, gbHardware);
   utilWriteIntMem(data, gbBlackScreen);
   utilWriteIntMem(data, oldRegister_WY);
   utilWriteIntMem(data, gbWindowLine);
   utilWriteIntMem(data, inUseRegister_WY);
   utilWriteIntMem(data, gbScreenOn);
   utilWriteIntMem(data, 0x12345678); // end marker

   return (ptrdiff_t)data - (ptrdiff_t)orig;
}

bool gbReadSaveState(const uint8_t* data, unsigned)
{
   int version = utilReadIntMem(data);

   if (version != GBSAVE_GAME_VERSION)
   {
      systemMessage(MSG_UNSUPPORTED_VB_SGM,
          N_("Unsupported VBA-M save game version %d"), version);
      return false;
   }

   uint8_t romname[20];

   utilReadMem(romname, data, 15);

   if (memcmp(&gbRom[0x134], romname, 15) != 0)
   {
      systemMessage(MSG_CANNOT_LOAD_SGM_FOR,
          N_("Cannot load save game for %s. Playing %s"),
          romname, &gbRom[0x134]);
      return false;
   }

   bool ub = false;
   bool ib = false;
   ub = utilReadIntMem(data) ? true : false;
   ib = utilReadIntMem(data) ? true : false;

   if ((ub != useBios) && (ib))
   {
      if (useBios)
         systemMessage(MSG_SAVE_GAME_NOT_USING_BIOS,
             N_("Save game is not using the BIOS files"));
      else
         systemMessage(MSG_SAVE_GAME_USING_BIOS,
             N_("Save game is using the BIOS file"));
      return false;
   }

   gbReset();

   inBios = ib;

   utilReadDataMem(data, gbSaveGameStruct);

   // Correct crash when loading color gameboy save in regular gameboy type.
   if (!gbCgbMode)
   {
      if (gbVram != NULL)
      {
         free(gbVram);
         gbVram = NULL;
      }
      if (gbWram != NULL)
      {
         free(gbWram);
         gbWram = NULL;
      }
   }
   else
   {
      if (gbVram == NULL)
         gbVram = (uint8_t*)malloc(0x4000);
      if (gbWram == NULL)
         gbWram = (uint8_t*)malloc(0x8000);
      memset(gbVram, 0, 0x4000);
      memset(gbPalette, 0, 2 * 128);
   }

   utilReadMem(&IFF, data, 2);

   if (gbSgbMode)
      gbSgbReadGame(data, version);
   else
      gbSgbMask = 0; // loading a game at the wrong time causes no display

   gbMbcReadGame(data, version);

   utilReadMem(gbPalette, data, 128 * sizeof(uint16_t));
   utilReadMem(&gbMemory[0x8000], data, 0x8000);

   if (gbRamSize && gbRam)
   {
      int ramSize = utilReadIntMem(data);
      utilReadMem(gbRam, data, (gbRamSize > ramSize) ? ramSize : gbRamSize); //read
      /*if (ramSize > gbRamSize)
            utilGzSeek(gzFile, ramSize - gbRamSize, SEEK_CUR);*/
      // Libretro Note: ????
   }

   memset(gbSCYLine, gbMemory[REG_SCY], sizeof(gbSCYLine));
   memset(gbSCXLine, gbMemory[REG_SCX], sizeof(gbSCXLine));
   memset(gbBgpLine, (gbBgp[0] | (gbBgp[1] << 2) | (gbBgp[2] << 4) | (gbBgp[3] << 6)), sizeof(gbBgpLine));
   memset(gbObp0Line, (gbObp0[0] | (gbObp0[1] << 2) | (gbObp0[2] << 4) | (gbObp0[3] << 6)), sizeof(gbObp0Line));
   memset(gbObp1Line, (gbObp1[0] | (gbObp1[1] << 2) | (gbObp1[2] << 4) | (gbObp1[3] << 6)), sizeof(gbObp1Line));
   memset(gbSpritesTicks, 0x0, sizeof(gbSpritesTicks));

   if (inBios)
   {
      gbMemoryMap[GB_MEM_CART_BANK0] = &gbMemory[0x0000];
      if (gbHardware & 5)
      {
         memcpy((uint8_t*)(gbMemory), (uint8_t*)(gbRom), 0x1000);
         memcpy((uint8_t*)(gbMemory), (uint8_t*)(bios), 0x100);
      }
      else if (gbHardware & 2)
      {
         memcpy((uint8_t*)(gbMemory), (uint8_t*)(bios), 0x900);
         memcpy((uint8_t*)(gbMemory + 0x100), (uint8_t*)(gbRom + 0x100), 0x100);
      }
   }
   else
   {
      gbMemoryMap[GB_MEM_CART_BANK0] = &gbRom[0x0000];
   }

   gbMemoryMap[GB_MEM_CART_BANK0 + 1] = &gbRom[0x1000];
   gbMemoryMap[GB_MEM_CART_BANK0 + 2] = &gbRom[0x2000];
   gbMemoryMap[GB_MEM_CART_BANK0 + 3] = &gbRom[0x3000];
   gbMemoryMap[GB_MEM_CART_BANK1] = &gbRom[0x4000];
   gbMemoryMap[GB_MEM_CART_BANK1 + 1] = &gbRom[0x5000];
   gbMemoryMap[GB_MEM_CART_BANK1 + 2] = &gbRom[0x6000];
   gbMemoryMap[GB_MEM_CART_BANK1 + 3] = &gbRom[0x7000];
   gbMemoryMap[GB_MEM_VRAM] = &gbMemory[0x8000];
   gbMemoryMap[GB_MEM_VRAM + 1] = &gbMemory[0x9000];
   gbMemoryMap[GB_MEM_EXTERNAL_RAM] = &gbMemory[0xa000];
   gbMemoryMap[GB_MEM_EXTERNAL_RAM + 1] = &gbMemory[0xb000];
   gbMemoryMap[GB_MEM_WRAM_BANK0] = &gbMemory[0xc000];
   gbMemoryMap[GB_MEM_WRAM_BANK1] = &gbMemory[0xd000];
   gbMemoryMap[GB_MEM_BASE_E] = &gbMemory[0xe000];
   gbMemoryMap[GB_MEM_BASE_F] = &gbMemory[0xf000];

   mapperUpdateMap();

   if (gbCgbMode)
   {
      utilReadMem(gbVram, data, 0x4000);
      utilReadMem(gbWram, data, 0x8000);

      int bank = gbMemory[REG_SVBK];
      if (bank == 0)
         bank = 1;
      gbMemoryMap[GB_MEM_WRAM_BANK0] = &gbWram[0x0000];
      gbMemoryMap[GB_MEM_WRAM_BANK1] = &gbWram[bank << 12];

      bank = gbMemory[REG_VBK] << 13;
      gbMemoryMap[GB_MEM_VRAM] = &gbVram[bank];
      gbMemoryMap[GB_MEM_VRAM + 1] = &gbVram[bank + 0x1000];

   }

   gbSoundReadGame(data, version);

   if (gbCgbMode && gbSgbMode)
   {
      gbSgbMode = 0;
   }

   if (gbBorderOn && !gbSgbMask)
   {
      gbSgbRenderBorder();
   }

   gbLcdModeDelayed = utilReadIntMem(data);
   gbLcdTicksDelayed = utilReadIntMem(data);
   gbLcdLYIncrementTicksDelayed = utilReadIntMem(data);
   gbSpritesTicks[299] = utilReadIntMem(data) & 0xff;
   gbTimerModeChange = (utilReadIntMem(data) ? true : false);
   gbTimerOnChange = (utilReadIntMem(data) ? true : false);
   gbHardware = utilReadIntMem(data);
   gbBlackScreen = (utilReadIntMem(data) ? true : false);
   oldRegister_WY = utilReadIntMem(data);
   gbWindowLine = utilReadIntMem(data);
   inUseRegister_WY = utilReadIntMem(data);
   gbScreenOn = (utilReadIntMem(data) ? true : false);

   if (gbSpeed)
      gbLine99Ticks *= 2;

   systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

   if (version >= 12 && utilReadIntMem(data) != 0x12345678)
      assert(false); // fails if something read too much/little from file

   return true;
}

bool gbWriteMemSaveState(char*, int, long&)
{
   return false;
}

bool gbReadMemSaveState(char*, int)
{
   return false;
}

bool gbReadBatteryFile(const char*)
{
   return false;
}

bool gbWriteBatteryFile(const char*)
{
   return false;
}

struct EmulatedSystem GBSystem = {
   // emuMain
   gbEmulate,
   // emuReset
   gbReset,
   // emuCleanUp
   gbCleanUp,
   // emuReadBattery
   gbReadBatteryFile,
   // emuWriteBattery
   gbWriteBatteryFile,
   // emuReadState
   gbReadSaveState,
   // emuWriteState
   gbWriteSaveState,
   // emuReadMemState
   gbReadMemSaveState,
   // emuWriteMemState
   gbWriteMemSaveState,
   // emuWritePNG
   gbWritePNGFile,
   // emuWriteBMP
   gbWriteBMPFile,
   // emuUpdateCPSR
   NULL,
   // emuFlushAudio
   gbSoundEndFrame,
   // emuHasDebugger
   false,
// emuCount
#ifdef FINAL_VERSION
   72000 / 2,
#else
   1000,
#endif
};
