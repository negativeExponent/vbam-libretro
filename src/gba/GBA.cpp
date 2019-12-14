//#include <memory.h>
//#include <stdarg.h>
//#include <stddef.h>
//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>
#ifndef _MSC_VER
//#include <strings.h>
#endif

#include "GBA.h"
#include "../NLS.h"
#include "../System.h"
#include "../Util.h"
#include "../common/ConfigManager.h"
#include "../common/Port.h"
#include "Cheats.h"
#include "EEprom.h"
#include "Flash.h"
#include "GBAGfx.h"
#include "GBALink.h"
#include "GBAcpu.h"
#include "GBAinline.h"
#include "Globals.h"
#include "Sound.h"
#include "Sram.h"
#include "bios.h"
#include "ereader.h"

#ifdef __GNUC__
#define _stricmp strcasecmp
#endif

extern int emulating;
int SWITicks = 0;
int IRQTicks = 0;
uint32_t mastercode = 0;
int layerEnableDelay = 0;
bool busPrefetch = false;
bool busPrefetchEnable = false;
uint32_t busPrefetchCount = 0;
int cpuDmaTicksToUpdate = 0;
int cpuDmaCount = 0;
bool cpuDmaHack = false;
uint32_t cpuDmaLast = 0;
int dummyAddress = 0;
bool cpuBreakLoop = false;
int cpuNextEvent = 0;
bool intState = false;
bool stopState = false;
bool holdState = false;
int holdType = 0;
bool cpuSramEnabled = true;
bool cpuFlashEnabled = true;
bool cpuEEPROMEnabled = true;
bool cpuEEPROMSensorEnabled = false;
int cpuTotalTicks = 0;
int lcdTicks = (useBios && !skipBios) ? 1008 : 208;
bool fxOn = false;
bool windowOn = false;
gba_dma_t dma[4];
gba_timers_t timers;

#ifdef BKPT_SUPPORT
uint8_t freezeWorkRAM[SIZE_WRAM];
uint8_t freezeInternalRAM[SIZE_IRAM];
uint8_t freezeVRAM[0x18000];
uint8_t freezePRAM[SIZE_PRAM];
uint8_t freezeOAM[SIZE_OAM];
bool debugger_last;
#endif

void (*cpuSaveGameFunc)(uint32_t, uint8_t) = flashSaveDecide;
uint32_t cpuPrefetch[2];
const int TIMER_TICKS[4] = { 0, 6, 8, 10 };
uint8_t memoryWait[16] = { 0, 0, 2, 0, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 0 };
uint8_t memoryWait32[16] = { 0, 0, 5, 0, 0, 1, 1, 0, 7, 7, 9, 9, 13, 13, 4, 0 };
uint8_t memoryWaitSeq[16] = { 0, 0, 2, 0, 0, 0, 0, 0, 2, 2, 4, 4, 8, 8, 4, 0 };
uint8_t memoryWaitSeq32[16] = { 0, 0, 5, 0, 0, 1, 1, 0, 5, 5, 9, 9, 17, 17, 4, 0 };

// The videoMemoryWait constants are used to add some waitstates
// if the opcode access video memory data outside of vblank/hblank
// It seems to happen on only one ticks for each pixel.
// Not used for now (too problematic with current code).
//const uint8_t videoMemoryWait[16] =
//  {0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};

uint8_t biosProtected[4];

static lcd_renderer_t renderer;

#ifdef MSB_FIRST
bool cpuBiosSwapped = false;
#endif

uint32_t myROM[] = {
   0xEA000006,
   0xEA000093,
   0xEA000006,
   0x00000000,
   0x00000000,
   0x00000000,
   0xEA000088,
   0x00000000,
   0xE3A00302,
   0xE1A0F000,
   0xE92D5800,
   0xE55EC002,
   0xE28FB03C,
   0xE79BC10C,
   0xE14FB000,
   0xE92D0800,
   0xE20BB080,
   0xE38BB01F,
   0xE129F00B,
   0xE92D4004,
   0xE1A0E00F,
   0xE12FFF1C,
   0xE8BD4004,
   0xE3A0C0D3,
   0xE129F00C,
   0xE8BD0800,
   0xE169F00B,
   0xE8BD5800,
   0xE1B0F00E,
   0x0000009C,
   0x0000009C,
   0x0000009C,
   0x0000009C,
   0x000001F8,
   0x000001F0,
   0x000000AC,
   0x000000A0,
   0x000000FC,
   0x00000168,
   0xE12FFF1E,
   0xE1A03000,
   0xE1A00001,
   0xE1A01003,
   0xE2113102,
   0x42611000,
   0xE033C040,
   0x22600000,
   0xE1B02001,
   0xE15200A0,
   0x91A02082,
   0x3AFFFFFC,
   0xE1500002,
   0xE0A33003,
   0x20400002,
   0xE1320001,
   0x11A020A2,
   0x1AFFFFF9,
   0xE1A01000,
   0xE1A00003,
   0xE1B0C08C,
   0x22600000,
   0x42611000,
   0xE12FFF1E,
   0xE92D0010,
   0xE1A0C000,
   0xE3A01001,
   0xE1500001,
   0x81A000A0,
   0x81A01081,
   0x8AFFFFFB,
   0xE1A0000C,
   0xE1A04001,
   0xE3A03000,
   0xE1A02001,
   0xE15200A0,
   0x91A02082,
   0x3AFFFFFC,
   0xE1500002,
   0xE0A33003,
   0x20400002,
   0xE1320001,
   0x11A020A2,
   0x1AFFFFF9,
   0xE0811003,
   0xE1B010A1,
   0xE1510004,
   0x3AFFFFEE,
   0xE1A00004,
   0xE8BD0010,
   0xE12FFF1E,
   0xE0010090,
   0xE1A01741,
   0xE2611000,
   0xE3A030A9,
   0xE0030391,
   0xE1A03743,
   0xE2833E39,
   0xE0030391,
   0xE1A03743,
   0xE2833C09,
   0xE283301C,
   0xE0030391,
   0xE1A03743,
   0xE2833C0F,
   0xE28330B6,
   0xE0030391,
   0xE1A03743,
   0xE2833C16,
   0xE28330AA,
   0xE0030391,
   0xE1A03743,
   0xE2833A02,
   0xE2833081,
   0xE0030391,
   0xE1A03743,
   0xE2833C36,
   0xE2833051,
   0xE0030391,
   0xE1A03743,
   0xE2833CA2,
   0xE28330F9,
   0xE0000093,
   0xE1A00840,
   0xE12FFF1E,
   0xE3A00001,
   0xE3A01001,
   0xE92D4010,
   0xE3A03000,
   0xE3A04001,
   0xE3500000,
   0x1B000004,
   0xE5CC3301,
   0xEB000002,
   0x0AFFFFFC,
   0xE8BD4010,
   0xE12FFF1E,
   0xE3A0C301,
   0xE5CC3208,
   0xE15C20B8,
   0xE0110002,
   0x10222000,
   0x114C20B8,
   0xE5CC4208,
   0xE12FFF1E,
   0xE92D500F,
   0xE3A00301,
   0xE1A0E00F,
   0xE510F004,
   0xE8BD500F,
   0xE25EF004,
   0xE59FD044,
   0xE92D5000,
   0xE14FC000,
   0xE10FE000,
   0xE92D5000,
   0xE3A0C302,
   0xE5DCE09C,
   0xE35E00A5,
   0x1A000004,
   0x05DCE0B4,
   0x021EE080,
   0xE28FE004,
   0x159FF018,
   0x059FF018,
   0xE59FD018,
   0xE8BD5000,
   0xE169F00C,
   0xE8BD5000,
   0xE25EF004,
   0x03007FF0,
   0x09FE2000,
   0x09FFC000,
   0x03007FE0
};

static uint16_t dummy_state_u16;

#define SKIP_u16(name      )              \
   {                                      \
       &dummy_state_u16, sizeof(uint16_t) \
   }

variable_desc saveGameStruct[] = {
   // IO:LCD
   { &lcd.dispcnt, sizeof(uint16_t) },
   { &lcd.dispstat, sizeof(uint16_t) },
   { &lcd.vcount, sizeof(uint16_t) },
   SKIP_u16(BG0CNT),
   SKIP_u16(BG1CNT),
   SKIP_u16(BG2CNT),
   SKIP_u16(BG3CNT),
   { &lcd.bg[0].hofs, sizeof(uint16_t) },
   { &lcd.bg[0].vofs, sizeof(uint16_t) },
   { &lcd.bg[1].hofs, sizeof(uint16_t) },
   { &lcd.bg[1].vofs, sizeof(uint16_t) },
   { &lcd.bg[2].hofs, sizeof(uint16_t) },
   { &lcd.bg[2].vofs, sizeof(uint16_t) },
   { &lcd.bg[3].hofs, sizeof(uint16_t) },
   { &lcd.bg[3].vofs, sizeof(uint16_t) },
   { &lcd.bg[2].dx, sizeof(int16_t) },
   { &lcd.bg[2].dmx, sizeof(int16_t) },
   { &lcd.bg[2].dy, sizeof(int16_t) },
   { &lcd.bg[2].dmy, sizeof(int16_t) },
   { &lcd.bg[2].x_ref, sizeof(int32_t) },
   { &lcd.bg[2].y_ref, sizeof(int32_t) },   
   { &lcd.bg[3].dx, sizeof(int16_t) },
   { &lcd.bg[3].dmx, sizeof(int16_t) },
   { &lcd.bg[3].dy, sizeof(int16_t) },
   { &lcd.bg[3].dmy, sizeof(int16_t) },
   { &lcd.bg[3].x_ref, sizeof(int32_t) },
   { &lcd.bg[3].y_ref, sizeof(int32_t) },
   { &lcd.winh[0], sizeof(uint16_t) },
   { &lcd.winh[1], sizeof(uint16_t) },
   { &lcd.winv[0], sizeof(uint16_t) },
   { &lcd.winv[1], sizeof(uint16_t) },
   { &lcd.winin, sizeof(uint16_t) },
   { &lcd.winout, sizeof(uint16_t) },
   SKIP_u16(MOSAIC),
   { &lcd.bldcnt, sizeof(uint16_t) },
   { &lcd.bldalpha, sizeof(uint16_t) },
   { &lcd.bldy, sizeof(uint16_t) },

   // IO:DMA
   { &dma[0].DMASAD_L, sizeof(uint16_t) },
   { &dma[0].DMASAD_H, sizeof(uint16_t) },
   { &dma[0].DMADAD_L, sizeof(uint16_t) },
   { &dma[0].DMADAD_H, sizeof(uint16_t) },
   { &dma[0].DMACNT_L, sizeof(uint16_t) },
   { &dma[0].DMACNT_H, sizeof(uint16_t) },
   { &dma[1].DMASAD_L, sizeof(uint16_t) },
   { &dma[1].DMASAD_H, sizeof(uint16_t) },
   { &dma[1].DMADAD_L, sizeof(uint16_t) },
   { &dma[1].DMADAD_H, sizeof(uint16_t) },
   { &dma[1].DMACNT_L, sizeof(uint16_t) },
   { &dma[1].DMACNT_H, sizeof(uint16_t) },
   { &dma[2].DMASAD_L, sizeof(uint16_t) },
   { &dma[2].DMASAD_H, sizeof(uint16_t) },
   { &dma[2].DMADAD_L, sizeof(uint16_t) },
   { &dma[2].DMADAD_H, sizeof(uint16_t) },
   { &dma[2].DMACNT_L, sizeof(uint16_t) },
   { &dma[2].DMACNT_H, sizeof(uint16_t) },
   { &dma[3].DMASAD_L, sizeof(uint16_t) },
   { &dma[3].DMASAD_H, sizeof(uint16_t) },
   { &dma[3].DMADAD_L, sizeof(uint16_t) },
   { &dma[3].DMADAD_H, sizeof(uint16_t) },
   { &dma[3].DMACNT_L, sizeof(uint16_t) },
   { &dma[3].DMACNT_H, sizeof(uint16_t) },

   // IO:TIMER
   { &timers.tm[0].TMCNT_L, sizeof(uint16_t) },
   { &timers.tm[0].TMCNT_H, sizeof(uint16_t) },
   { &timers.tm[1].TMCNT_L, sizeof(uint16_t) },
   { &timers.tm[1].TMCNT_H, sizeof(uint16_t) },
   { &timers.tm[2].TMCNT_L, sizeof(uint16_t) },
   { &timers.tm[2].TMCNT_H, sizeof(uint16_t) },
   { &timers.tm[3].TMCNT_L, sizeof(uint16_t) },
   { &timers.tm[3].TMCNT_H, sizeof(uint16_t) },

   // OTHERS
   { &P1, sizeof(uint16_t) },
   { &IE, sizeof(uint16_t) },
   { &IF, sizeof(uint16_t) },
   { &IME, sizeof(uint16_t) },

   /* === end of IO REGS === */

   { &holdState, sizeof(bool) },
   { &holdType, sizeof(int) },
   { &lcdTicks, sizeof(int) },
   { &timers.tm[0].On, sizeof(bool) },
   { &timers.tm[0].Ticks, sizeof(int) },
   { &timers.tm[0].Reload, sizeof(int) },
   { &timers.tm[0].ClockReload, sizeof(int) },
   { &timers.tm[1].On, sizeof(bool) },
   { &timers.tm[1].Ticks, sizeof(int) },
   { &timers.tm[1].Reload, sizeof(int) },
   { &timers.tm[1].ClockReload, sizeof(int) },
   { &timers.tm[2].On, sizeof(bool) },
   { &timers.tm[2].Ticks, sizeof(int) },
   { &timers.tm[2].Reload, sizeof(int) },
   { &timers.tm[2].ClockReload, sizeof(int) },
   { &timers.tm[3].On, sizeof(bool) },
   { &timers.tm[3].Ticks, sizeof(int) },
   { &timers.tm[3].Reload, sizeof(int) },
   { &timers.tm[3].ClockReload, sizeof(int) },
   { &dma[0].Source, sizeof(uint32_t) },
   { &dma[0].Dest, sizeof(uint32_t) },
   { &dma[1].Source, sizeof(uint32_t) },
   { &dma[1].Dest, sizeof(uint32_t) },
   { &dma[2].Source, sizeof(uint32_t) },
   { &dma[2].Dest, sizeof(uint32_t) },
   { &dma[3].Source, sizeof(uint32_t) },
   { &dma[3].Dest, sizeof(uint32_t) },
   { &fxOn, sizeof(bool) },
   { &windowOn, sizeof(bool) },
   { &N_FLAG, sizeof(bool) },
   { &C_FLAG, sizeof(bool) },
   { &Z_FLAG, sizeof(bool) },
   { &V_FLAG, sizeof(bool) },
   { &armState, sizeof(bool) },
   { &armIrqEnable, sizeof(bool) },
   { &armNextPC, sizeof(uint32_t) },
   { &armMode, sizeof(int) },
   { &saveType, sizeof(int) },
   { NULL, 0 }
};

inline int CPUUpdateTicks()
{
   int cpuLoopTicks = lcdTicks;

   //if (soundTicks < cpuLoopTicks)
      //cpuLoopTicks = soundTicks;

   if (timers.tm[0].On && (timers.tm[0].Ticks < cpuLoopTicks))
   {
      cpuLoopTicks = timers.tm[0].Ticks;
   }
   if (timers.tm[1].On && !(timers.tm[1].TMCNT_H & 4) && (timers.tm[1].Ticks < cpuLoopTicks))
   {
      cpuLoopTicks = timers.tm[1].Ticks;
   }
   if (timers.tm[2].On && !(timers.tm[2].TMCNT_H & 4) && (timers.tm[2].Ticks < cpuLoopTicks))
   {
      cpuLoopTicks = timers.tm[2].Ticks;
   }
   if (timers.tm[3].On && !(timers.tm[3].TMCNT_H & 4) && (timers.tm[3].Ticks < cpuLoopTicks))
   {
      cpuLoopTicks = timers.tm[3].Ticks;
   }

   if (SWITicks)
   {
      if (SWITicks < cpuLoopTicks)
         cpuLoopTicks = SWITicks;
   }

   if (IRQTicks)
   {
      if (IRQTicks < cpuLoopTicks)
         cpuLoopTicks = IRQTicks;
   }

   return cpuLoopTicks;
}

#define CLEAR_ARRAY(a)              \
   {                                \
      uint32_t* array = (a);        \
      for (int i = 0; i < 240; i++) \
      {                             \
         *array++ = 0x80000000;     \
      }                             \
   }

void CPUUpdateRenderBuffers(bool force)
{
   if (!(layerEnable & 0x0100) || force)
   {
      CLEAR_ARRAY(lineBG[0]);
   }
   if (!(layerEnable & 0x0200) || force)
   {
      CLEAR_ARRAY(lineBG[1]);
   }
   if (!(layerEnable & 0x0400) || force)
   {
      CLEAR_ARRAY(lineBG[2]);
   }
   if (!(layerEnable & 0x0800) || force)
   {
      CLEAR_ARRAY(lineBG[3]);
   }
}

#include <stddef.h>
unsigned int CPUWriteState(uint8_t* data, unsigned size)
{
   uint8_t* orig = data;

   utilWriteIntMem(data, SAVE_GAME_VERSION);
   utilWriteMem(data, &rom[0xa0], 16);
   utilWriteIntMem(data, useBios);
   utilWriteMem(data, &reg[0], sizeof(reg));

   utilWriteDataMem(data, saveGameStruct);

   utilWriteIntMem(data, stopState);
   utilWriteIntMem(data, IRQTicks);

   utilWriteMem(data, internalRAM, SIZE_IRAM);
   utilWriteMem(data, paletteRAM, SIZE_PRAM);
   utilWriteMem(data, workRAM, SIZE_WRAM);
   utilWriteMem(data, vram, SIZE_VRAM);
   utilWriteMem(data, oam, SIZE_OAM);
   utilWriteMem(data, pix, SIZE_PIX);
   utilWriteMem(data, ioMem, SIZE_IOMEM);

   eepromSaveGame(data);
   flashSaveGame(data);
   soundSaveGame(data);
   rtcSaveGame(data);

   return (ptrdiff_t)data - (ptrdiff_t)orig;
}

bool CPUWriteMemState(char* memory, int available, long& reserved)
{
   return false;
}

bool CPUReadState(const uint8_t* data, unsigned size)
{
   // Don't really care about version.
   int version = utilReadIntMem(data);
   if (version != SAVE_GAME_VERSION)
      return false;

   char romname[16];
   utilReadMem(romname, data, 16);
   if (memcmp(&rom[0xa0], romname, 16) != 0)
      return false;

   // Don't care about use bios ...
   utilReadIntMem(data);

   utilReadMem(&reg[0], data, sizeof(reg));

   utilReadDataMem(data, saveGameStruct);

   stopState = utilReadIntMem(data) ? true : false;

   IRQTicks = utilReadIntMem(data);
   if (IRQTicks > 0)
      intState = true;
   else
   {
      intState = false;
      IRQTicks = 0;
   }

   utilReadMem(internalRAM, data, SIZE_IRAM);
   utilReadMem(paletteRAM, data, SIZE_PRAM);
   utilReadMem(workRAM, data, SIZE_WRAM);
   utilReadMem(vram, data, SIZE_VRAM);
   utilReadMem(oam, data, SIZE_OAM);
   utilReadMem(pix, data, SIZE_PIX);
   utilReadMem(ioMem, data, SIZE_IOMEM);

   eepromReadGame(data, version);
   flashReadGame(data, version);
   soundReadGame(data, version);
   rtcReadGame(data);

   //// Copypasta stuff ...
   // set pointers!
   layerEnable = layerSettings & lcd.dispcnt;

   CPUUpdateRender();

   // CPU Update Render Buffers set to true
   CLEAR_ARRAY(lineBG[0]);
   CLEAR_ARRAY(lineBG[1]);
   CLEAR_ARRAY(lineBG[2]);
   CLEAR_ARRAY(lineBG[3]);
   // End of CPU Update Render Buffers set to true

   LCDUpdateWindow0();
   LCDUpdateWindow1();
   LCDUpdateBGRegisters();

   oam_updated = true;
   for (int x = 0; x < 128; x++) oam_obj_updated[x] = true;

   SetSaveType(saveType);

   systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
   if (armState)
   {
      ARM_PREFETCH;
   }
   else
   {
      THUMB_PREFETCH;
   }

   CPUUpdateRegister(0x204, CPUReadHalfWordQuick(0x4000204));

   return true;
}

bool CPUIsGBABios(const char* file)
{
   if (strlen(file) > 4)
   {
      const char* p = strrchr(file, '.');

      if (p != NULL)
      {
         if (_stricmp(p, ".gba") == 0)
            return true;
         if (_stricmp(p, ".agb") == 0)
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

void CPUCleanUp()
{
   GBAMemoryCleanup();

   systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

   emulating = 0;
}

void SetMapMasks()
{
   map[0].mask = 0x3FFF;
   map[2].mask = 0x3FFFF;
   map[3].mask = 0x7FFF;
   map[4].mask = 0x3FF;
   map[5].mask = 0x3FF;
   map[6].mask = 0x1FFFF;
   map[7].mask = 0x3FF;
   map[8].mask = 0x1FFFFFF;
   map[9].mask = 0x1FFFFFF;
   map[10].mask = 0x1FFFFFF;
   map[12].mask = 0x1FFFFFF;
   map[14].mask = 0xFFFF;

#ifdef BKPT_SUPPORT
   for (int i = 0; i < 16; i++)
   {
      map[i].size = map[i].mask + 1;
      if (map[i].size > 0)
      {
         map[i].trace = (uint8_t*)calloc(map[i].size >> 3, sizeof(uint8_t));

         map[i].breakPoints = (uint8_t*)calloc(map[i].size >> 1, sizeof(uint8_t));

         if (map[i].trace == NULL || map[i].breakPoints == NULL)
         {
            systemMessage(MSG_OUT_OF_MEMORY, N_("Failed to allocate memory for %s"),
                "TRACE");
         }
      }
      else
      {
         map[i].trace = NULL;
         map[i].breakPoints = NULL;
      }
   }
   clearBreakRegList();
#endif
}

int CPULoadRom(const char* szFile)
{
   if (!GBAMemoryInit())
   {
      CPUCleanUp();
      return 0;
   }

   romSize = SIZE_ROM;
   uint8_t* whereToLoad = cpuIsMultiBoot ? workRAM : rom;

   if (szFile != NULL)
   {
      if (!utilLoad(szFile,
              utilIsGBAImage,
              whereToLoad,
              romSize))
      {
         free(rom);
         rom = NULL;
         free(workRAM);
         workRAM = NULL;
         return 0;
      }
   }

   uint16_t* temp = (uint16_t*)(rom + ((romSize + 1) & ~1));
   int i;
   for (i = (romSize + 1) & ~1; i < SIZE_ROM; i += 2)
   {
      WRITE16LE(temp, (i >> 1) & 0xFFFF);
      temp++;
   }

   return romSize;
}

int CPULoadRomData(const char* data, int size)
{
   if (!GBAMemoryInit())
   {
      CPUCleanUp();
      return 0;
   }

   romSize = SIZE_ROM;
   uint8_t* whereToLoad = cpuIsMultiBoot ? workRAM : rom;

   romSize = size % 2 == 0 ? size : size + 1;
   memcpy(whereToLoad, data, size);

   uint16_t* temp = (uint16_t*)(rom + ((romSize + 1) & ~1));
   int i;
   for (i = (romSize + 1) & ~1; i < SIZE_ROM; i += 2)
   {
      WRITE16LE(temp, (i >> 1) & 0xFFFF);
      temp++;
   }

   return romSize;
}

void doMirroring(bool b)
{
   int romSizeRounded = romSize;
   romSizeRounded--;
   romSizeRounded |= romSizeRounded >> 1;
   romSizeRounded |= romSizeRounded >> 2;
   romSizeRounded |= romSizeRounded >> 4;
   romSizeRounded |= romSizeRounded >> 8;
   romSizeRounded |= romSizeRounded >> 16;
   romSizeRounded++;
   uint32_t mirroredRomSize = (((romSizeRounded) >> 20) & 0x3F) << 20;
   uint32_t mirroredRomAddress = mirroredRomSize;
   if ((mirroredRomSize <= 0x800000) && (b))
   {
      if (mirroredRomSize == 0)
         mirroredRomSize = 0x100000;
      while (mirroredRomAddress < 0x01000000)
      {
         memcpy((uint16_t*)(rom + mirroredRomAddress), (uint16_t*)(rom), mirroredRomSize);
         mirroredRomAddress += mirroredRomSize;
      }
   }
}

const char* GetLoadDotCodeFile()
{
   return loadDotCodeFile;
}

const char* GetSaveDotCodeFile()
{
   return saveDotCodeFile;
}

void SetLoadDotCodeFile(const char* szFile)
{
   loadDotCodeFile = strdup(szFile);
}

void SetSaveDotCodeFile(const char* szFile)
{
   saveDotCodeFile = strdup(szFile);
}

void CPUUpdateRender()
{
   int mode = (lcd.dispcnt & 7);
   renderer.mode = mode;
   if (!windowOn && !(layerEnable & 0x8000))
   {
      if (!fxOn)
      {
         renderer.type = 0;
      }
      else
      {
         renderer.type = 1;
      }
   }
   else
   {
      renderer.type = 2;
   }
}

void CPUUpdateCPSR()
{
   uint32_t CPSR = reg[16].I & 0x40;
   if (N_FLAG)
      CPSR |= 0x80000000;
   if (Z_FLAG)
      CPSR |= 0x40000000;
   if (C_FLAG)
      CPSR |= 0x20000000;
   if (V_FLAG)
      CPSR |= 0x10000000;
   if (!armState)
      CPSR |= 0x00000020;
   if (!armIrqEnable)
      CPSR |= 0x80;
   CPSR |= (armMode & 0x1F);
   reg[16].I = CPSR;
}

void CPUUpdateFlags(bool breakLoop)
{
   uint32_t CPSR = reg[16].I;

   N_FLAG = (CPSR & 0x80000000) ? true : false;
   Z_FLAG = (CPSR & 0x40000000) ? true : false;
   C_FLAG = (CPSR & 0x20000000) ? true : false;
   V_FLAG = (CPSR & 0x10000000) ? true : false;
   armState = (CPSR & 0x20) ? false : true;
   armIrqEnable = (CPSR & 0x80) ? false : true;
   if (breakLoop)
   {
      if (armIrqEnable && (IF & IE) && (IME & 1))
         cpuNextEvent = cpuTotalTicks;
   }
}

#ifdef MSB_FIRST
static void CPUSwap(volatile uint32_t* a, volatile uint32_t* b)
{
   volatile uint32_t c = *b;
   *b = *a;
   *a = c;
}
#else
static void CPUSwap(uint32_t* a, uint32_t* b)
{
   uint32_t c = *b;
   *b = *a;
   *a = c;
}
#endif

void CPUSwitchMode(int mode, bool saveState, bool breakLoop)
{
   //  if(armMode == mode)
   //    return;

   CPUUpdateCPSR();

   switch (armMode)
   {
      case 0x10:
      case 0x1F:
         reg[R13_USR].I = reg[13].I;
         reg[R14_USR].I = reg[14].I;
         reg[17].I = reg[16].I;
         break;
      case 0x11:
         CPUSwap(&reg[R8_FIQ].I, &reg[8].I);
         CPUSwap(&reg[R9_FIQ].I, &reg[9].I);
         CPUSwap(&reg[R10_FIQ].I, &reg[10].I);
         CPUSwap(&reg[R11_FIQ].I, &reg[11].I);
         CPUSwap(&reg[R12_FIQ].I, &reg[12].I);
         reg[R13_FIQ].I = reg[13].I;
         reg[R14_FIQ].I = reg[14].I;
         reg[SPSR_FIQ].I = reg[17].I;
         break;
      case 0x12:
         reg[R13_IRQ].I = reg[13].I;
         reg[R14_IRQ].I = reg[14].I;
         reg[SPSR_IRQ].I = reg[17].I;
         break;
      case 0x13:
         reg[R13_SVC].I = reg[13].I;
         reg[R14_SVC].I = reg[14].I;
         reg[SPSR_SVC].I = reg[17].I;
         break;
      case 0x17:
         reg[R13_ABT].I = reg[13].I;
         reg[R14_ABT].I = reg[14].I;
         reg[SPSR_ABT].I = reg[17].I;
         break;
      case 0x1b:
         reg[R13_UND].I = reg[13].I;
         reg[R14_UND].I = reg[14].I;
         reg[SPSR_UND].I = reg[17].I;
         break;
   }

   uint32_t CPSR = reg[16].I;
   uint32_t SPSR = reg[17].I;

   switch (mode)
   {
      case 0x10:
      case 0x1F:
         reg[13].I = reg[R13_USR].I;
         reg[14].I = reg[R14_USR].I;
         reg[16].I = SPSR;
         break;
      case 0x11:
         CPUSwap(&reg[8].I, &reg[R8_FIQ].I);
         CPUSwap(&reg[9].I, &reg[R9_FIQ].I);
         CPUSwap(&reg[10].I, &reg[R10_FIQ].I);
         CPUSwap(&reg[11].I, &reg[R11_FIQ].I);
         CPUSwap(&reg[12].I, &reg[R12_FIQ].I);
         reg[13].I = reg[R13_FIQ].I;
         reg[14].I = reg[R14_FIQ].I;
         if (saveState)
            reg[17].I = CPSR;
         else
            reg[17].I = reg[SPSR_FIQ].I;
         break;
      case 0x12:
         reg[13].I = reg[R13_IRQ].I;
         reg[14].I = reg[R14_IRQ].I;
         reg[16].I = SPSR;
         if (saveState)
            reg[17].I = CPSR;
         else
            reg[17].I = reg[SPSR_IRQ].I;
         break;
      case 0x13:
         reg[13].I = reg[R13_SVC].I;
         reg[14].I = reg[R14_SVC].I;
         reg[16].I = SPSR;
         if (saveState)
            reg[17].I = CPSR;
         else
            reg[17].I = reg[SPSR_SVC].I;
         break;
      case 0x17:
         reg[13].I = reg[R13_ABT].I;
         reg[14].I = reg[R14_ABT].I;
         reg[16].I = SPSR;
         if (saveState)
            reg[17].I = CPSR;
         else
            reg[17].I = reg[SPSR_ABT].I;
         break;
      case 0x1b:
         reg[13].I = reg[R13_UND].I;
         reg[14].I = reg[R14_UND].I;
         reg[16].I = SPSR;
         if (saveState)
            reg[17].I = CPSR;
         else
            reg[17].I = reg[SPSR_UND].I;
         break;
      default:
         systemMessage(MSG_UNSUPPORTED_ARM_MODE, N_("Unsupported ARM mode %02x"), mode);
         break;
   }
   armMode = mode;
   CPUUpdateFlags(breakLoop);
   CPUUpdateCPSR();
}

#define CPU_SOFTWARE_INTERRUPT()                \
   {                                            \
      uint32_t PC = reg[15].I;                  \
      bool savedArmState = armState;            \
      CPUSwitchMode(0x13, true, false);         \
      reg[14].I = PC - (savedArmState ? 4 : 2); \
      reg[15].I = 0x08;                         \
      armState = true;                          \
      armIrqEnable = false;                     \
      armNextPC = 0x08;                         \
      ARM_PREFETCH;                             \
      reg[15].I += 4;                           \
   }

void CPUSoftwareInterrupt(int comment)
{
   static bool disableMessage = false;
   if (armState)
      comment >>= 16;
#ifdef BKPT_SUPPORT
   if (comment == 0xff)
   {
      dbgOutput(NULL, reg[0].I);
      return;
   }
#endif
   if (comment == 0xfa)
   {
      //agbPrintFlush();
      return;
   }
#ifdef SDL
   if (comment == 0xf9)
   {
      emulating = 0;
      cpuNextEvent = cpuTotalTicks;
      cpuBreakLoop = true;
      return;
   }
#endif
   if (useBios)
   {
#ifdef GBA_LOGGING
      if (systemVerbose & VERBOSE_SWI)
      {
         log("SWI: %08x at %08x (0x%08x,0x%08x,0x%08x,VCOUNT = %2d)\n", comment,
             armState ? armNextPC - 4 : armNextPC - 2,
             reg[0].I,
             reg[1].I,
             reg[2].I,
             lcd.vcount);
      }
#endif
      if ((comment & 0xF8) != 0xE0)
      {
         CPU_SOFTWARE_INTERRUPT();
         return;
      }
      else
      {
         if (CheckEReaderRegion())
            BIOS_EReader_ScanCard(comment);
         else
            CPU_SOFTWARE_INTERRUPT();
         return;
      }
   }
   // This would be correct, but it causes problems if uncommented
   //  else {
   //    biosProtected = 0xe3a02004;
   //  }

   switch (comment)
   {
      case 0x00:
         BIOS_SoftReset();
         ARM_PREFETCH;
         break;
      case 0x01:
         BIOS_RegisterRamReset();
         break;
      case 0x02:
#ifdef GBA_LOGGING
         if (systemVerbose & VERBOSE_SWI)
         {
            /*log("Halt: (VCOUNT = %2d)\n",
          lcd.vcount);*/
         }
#endif
         holdState = true;
         holdType = -1;
         cpuNextEvent = cpuTotalTicks;
         break;
      case 0x03:
#ifdef GBA_LOGGING
         if (systemVerbose & VERBOSE_SWI)
         {
            /*log("Stop: (VCOUNT = %2d)\n",
          lcd.vcount);*/
         }
#endif
         holdState = true;
         holdType = -1;
         stopState = true;
         cpuNextEvent = cpuTotalTicks;
         break;
      case 0x04:
#ifdef GBA_LOGGING
         if (systemVerbose & VERBOSE_SWI)
         {
            log("IntrWait: 0x%08x,0x%08x (VCOUNT = %2d)\n",
                reg[0].I,
                reg[1].I,
                lcd.vcount);
         }
#endif
         CPU_SOFTWARE_INTERRUPT();
         break;
      case 0x05:
#ifdef GBA_LOGGING
         if (systemVerbose & VERBOSE_SWI)
         {
            log("VBlankIntrWait: (VCOUNT = %2d)\n",
                lcd.vcount);
         }
#endif
         CPU_SOFTWARE_INTERRUPT();
         break;
      case 0x06:
      case 0x07:
         CPU_SOFTWARE_INTERRUPT();
         break;
      case 0x08:
         BIOS_Sqrt();
         break;
      case 0x09:
         BIOS_ArcTan();
         break;
      case 0x0A:
         BIOS_ArcTan2();
         break;
      case 0x0B:
      {
         int len = (reg[2].I & 0x1FFFFF) >> 1;
         if (!(((reg[0].I & 0xe000000) == 0) || ((reg[0].I + len) & 0xe000000) == 0))
         {
            if ((reg[2].I >> 24) & 1)
            {
               if ((reg[2].I >> 26) & 1)
                  SWITicks = (7 + memoryWait32[(reg[1].I >> 24) & 0xF]) * (len >> 1);
               else
                  SWITicks = (8 + memoryWait[(reg[1].I >> 24) & 0xF]) * (len);
            }
            else
            {
               if ((reg[2].I >> 26) & 1)
                  SWITicks = (10 + memoryWait32[(reg[0].I >> 24) & 0xF] + memoryWait32[(reg[1].I >> 24) & 0xF]) * (len >> 1);
               else
                  SWITicks = (11 + memoryWait[(reg[0].I >> 24) & 0xF] + memoryWait[(reg[1].I >> 24) & 0xF]) * len;
            }
         }
      }
         BIOS_CpuSet();
         break;
      case 0x0C:
      {
         int len = (reg[2].I & 0x1FFFFF) >> 5;
         if (!(((reg[0].I & 0xe000000) == 0) || ((reg[0].I + len) & 0xe000000) == 0))
         {
            if ((reg[2].I >> 24) & 1)
               SWITicks = (6 + memoryWait32[(reg[1].I >> 24) & 0xF] + 7 * (memoryWaitSeq32[(reg[1].I >> 24) & 0xF] + 1)) * len;
            else
               SWITicks = (9 + memoryWait32[(reg[0].I >> 24) & 0xF] + memoryWait32[(reg[1].I >> 24) & 0xF] + 7 * (memoryWaitSeq32[(reg[0].I >> 24) & 0xF] + memoryWaitSeq32[(reg[1].I >> 24) & 0xF] + 2)) * len;
         }
      }
         BIOS_CpuFastSet();
         break;
      case 0x0D:
         BIOS_GetBiosChecksum();
         break;
      case 0x0E:
         BIOS_BgAffineSet();
         break;
      case 0x0F:
         BIOS_ObjAffineSet();
         break;
      case 0x10:
      {
         int len = CPUReadHalfWord(reg[2].I);
         if (!(((reg[0].I & 0xe000000) == 0) || ((reg[0].I + len) & 0xe000000) == 0))
            SWITicks = (32 + memoryWait[(reg[0].I >> 24) & 0xF]) * len;
      }
         BIOS_BitUnPack();
         break;
      case 0x11:
      {
         uint32_t len = CPUReadMemory(reg[0].I) >> 8;
         if (!(((reg[0].I & 0xe000000) == 0) || ((reg[0].I + (len & 0x1fffff)) & 0xe000000) == 0))
            SWITicks = (9 + memoryWait[(reg[1].I >> 24) & 0xF]) * len;
      }
         BIOS_LZ77UnCompWram();
         break;
      case 0x12:
      {
         uint32_t len = CPUReadMemory(reg[0].I) >> 8;
         if (!(((reg[0].I & 0xe000000) == 0) || ((reg[0].I + (len & 0x1fffff)) & 0xe000000) == 0))
            SWITicks = (19 + memoryWait[(reg[1].I >> 24) & 0xF]) * len;
      }
         BIOS_LZ77UnCompVram();
         break;
      case 0x13:
      {
         uint32_t len = CPUReadMemory(reg[0].I) >> 8;
         if (!(((reg[0].I & 0xe000000) == 0) || ((reg[0].I + (len & 0x1fffff)) & 0xe000000) == 0))
            SWITicks = (29 + (memoryWait[(reg[0].I >> 24) & 0xF] << 1)) * len;
      }
         BIOS_HuffUnComp();
         break;
      case 0x14:
      {
         uint32_t len = CPUReadMemory(reg[0].I) >> 8;
         if (!(((reg[0].I & 0xe000000) == 0) || ((reg[0].I + (len & 0x1fffff)) & 0xe000000) == 0))
            SWITicks = (11 + memoryWait[(reg[0].I >> 24) & 0xF] + memoryWait[(reg[1].I >> 24) & 0xF]) * len;
      }
         BIOS_RLUnCompWram();
         break;
      case 0x15:
      {
         uint32_t len = CPUReadMemory(reg[0].I) >> 9;
         if (!(((reg[0].I & 0xe000000) == 0) || ((reg[0].I + (len & 0x1fffff)) & 0xe000000) == 0))
            SWITicks = (34 + (memoryWait[(reg[0].I >> 24) & 0xF] << 1) + memoryWait[(reg[1].I >> 24) & 0xF]) * len;
      }
         BIOS_RLUnCompVram();
         break;
      case 0x16:
      {
         uint32_t len = CPUReadMemory(reg[0].I) >> 8;
         if (!(((reg[0].I & 0xe000000) == 0) || ((reg[0].I + (len & 0x1fffff)) & 0xe000000) == 0))
            SWITicks = (13 + memoryWait[(reg[0].I >> 24) & 0xF] + memoryWait[(reg[1].I >> 24) & 0xF]) * len;
      }
         BIOS_Diff8bitUnFilterWram();
         break;
      case 0x17:
      {
         uint32_t len = CPUReadMemory(reg[0].I) >> 9;
         if (!(((reg[0].I & 0xe000000) == 0) || ((reg[0].I + (len & 0x1fffff)) & 0xe000000) == 0))
            SWITicks = (39 + (memoryWait[(reg[0].I >> 24) & 0xF] << 1) + memoryWait[(reg[1].I >> 24) & 0xF]) * len;
      }
         BIOS_Diff8bitUnFilterVram();
         break;
      case 0x18:
      {
         uint32_t len = CPUReadMemory(reg[0].I) >> 9;
         if (!(((reg[0].I & 0xe000000) == 0) || ((reg[0].I + (len & 0x1fffff)) & 0xe000000) == 0))
            SWITicks = (13 + memoryWait[(reg[0].I >> 24) & 0xF] + memoryWait[(reg[1].I >> 24) & 0xF]) * len;
      }
         BIOS_Diff16bitUnFilter();
         break;
      case 0x19:
#ifdef GBA_LOGGING
         if (systemVerbose & VERBOSE_SWI)
         {
            log("SoundBiasSet: 0x%08x (VCOUNT = %2d)\n",
                reg[0].I,
                lcd.vcount);
         }
#endif
         if (reg[0].I)
            soundPause();
         else
            soundResume();
         break;
      case 0x1A:
         BIOS_SndDriverInit();
         SWITicks = 252000;
         break;
      case 0x1B:
         BIOS_SndDriverMode();
         SWITicks = 280000;
         break;
      case 0x1C:
         BIOS_SndDriverMain();
         SWITicks = 11050; //avg
         break;
      case 0x1D:
         BIOS_SndDriverVSync();
         SWITicks = 44;
         break;
      case 0x1E:
         BIOS_SndChannelClear();
         break;
      case 0x28:
         BIOS_SndDriverVSyncOff();
         break;
      case 0x1F:
         BIOS_MidiKey2Freq();
         break;
      case 0xE0:
      case 0xE1:
      case 0xE2:
      case 0xE3:
      case 0xE4:
      case 0xE5:
      case 0xE6:
      case 0xE7:
         if (CheckEReaderRegion())
            BIOS_EReader_ScanCard(comment);
         break;
      case 0x2A:
         BIOS_SndDriverJmpTableCopy();
      // let it go, because we don't really emulate this function
      default:
#ifdef GBA_LOGGING
         if (systemVerbose & VERBOSE_SWI)
         {
            log("SWI: %08x at %08x (0x%08x,0x%08x,0x%08x,VCOUNT = %2d)\n", comment,
                armState ? armNextPC - 4 : armNextPC - 2,
                reg[0].I,
                reg[1].I,
                reg[2].I,
                lcd.vcount);
         }
#endif

         if (!disableMessage)
         {
            systemMessage(MSG_UNSUPPORTED_BIOS_FUNCTION,
                N_("Unsupported BIOS function %02x called from %08x. A BIOS file is needed in order to get correct behaviour."),
                comment,
                armMode ? armNextPC - 4 : armNextPC - 2);
            disableMessage = true;
         }
         break;
   }
}

void CPUCompareVCOUNT(void)
{
   if (lcd.vcount == (lcd.dispstat >> 8))
   {
      lcd.dispstat |= 4;
      UPDATE_REG(REG_DISPSTAT, lcd.dispstat);

      if (lcd.dispstat & 0x20)
      {
         IF |= 4;
         UPDATE_REG(REG_IF, IF);
      }
   }
   else
   {
      lcd.dispstat &= 0xFFFB;
      UPDATE_REG(REG_DISPSTAT, lcd.dispstat);
   }
   if (layerEnableDelay > 0)
   {
      layerEnableDelay--;
      if (layerEnableDelay == 1)
         layerEnable = layerSettings & lcd.dispcnt;
   }
}

void doDMA(uint32_t& s, uint32_t& d, uint32_t si, uint32_t di, uint32_t c, int transfer32)
{
   int sm = s >> 24;
   int dm = d >> 24;
   int sw = 0;
   int dw = 0;
   int sc = c;

   cpuDmaHack = true;
   cpuDmaCount = c;
   // This is done to get the correct waitstates.
   if (sm > 15)
      sm = 15;
   if (dm > 15)
      dm = 15;

   //if ((sm>=0x05) && (sm<=0x07) || (dm>=0x05) && (dm <=0x07))
   //    blank = (((DISPSTAT | ((DISPSTAT>>1)&1))==1) ?  true : false);

   if (transfer32)
   {
      s &= 0xFFFFFFFC;
      if (s < 0x02000000 && (reg[15].I >> 24))
      {
         while (c != 0)
         {
            CPUWriteMemory(d, 0);
            d += di;
            c--;
         }
      }
      else
      {
         while (c != 0)
         {
            cpuDmaLast = CPUReadMemory(s);
            CPUWriteMemory(d, cpuDmaLast);
            d += di;
            s += si;
            c--;
         }
      }
   }
   else
   {
      s &= 0xFFFFFFFE;
      si = (int)si >> 1;
      di = (int)di >> 1;
      if (s < 0x02000000 && (reg[15].I >> 24))
      {
         while (c != 0)
         {
            CPUWriteHalfWord(d, 0);
            d += di;
            c--;
         }
      }
      else
      {
         while (c != 0)
         {
            cpuDmaLast = CPUReadHalfWord(s);
            CPUWriteHalfWord(d, cpuDmaLast);
            cpuDmaLast |= (cpuDmaLast << 16);
            d += di;
            s += si;
            c--;
         }
      }
   }

   cpuDmaCount = 0;

   int totalTicks = 0;

   if (transfer32)
   {
      sw = 1 + memoryWaitSeq32[sm & 15];
      dw = 1 + memoryWaitSeq32[dm & 15];
      totalTicks = (sw + dw) * (sc - 1) + 6 + memoryWait32[sm & 15] + memoryWaitSeq32[dm & 15];
   }
   else
   {
      sw = 1 + memoryWaitSeq[sm & 15];
      dw = 1 + memoryWaitSeq[dm & 15];
      totalTicks = (sw + dw) * (sc - 1) + 6 + memoryWait[sm & 15] + memoryWaitSeq[dm & 15];
   }

   cpuDmaTicksToUpdate += totalTicks;
   cpuDmaHack = false;
}

void CPUCheckDMA(int reason, int dmamask)
{
   // DMA 0
   if ((dma[0].DMACNT_H & 0x8000) && (dmamask & 1))
   {
      if (((dma[0].DMACNT_H >> 12) & 3) == reason)
      {
         uint32_t sourceIncrement = 4;
         uint32_t destIncrement = 4;
         switch ((dma[0].DMACNT_H >> 7) & 3)
         {
            case 0:
               break;
            case 1:
               sourceIncrement = (uint32_t)-4;
               break;
            case 2:
               sourceIncrement = 0;
               break;
         }
         switch ((dma[0].DMACNT_H >> 5) & 3)
         {
            case 0:
               break;
            case 1:
               destIncrement = (uint32_t)-4;
               break;
            case 2:
               destIncrement = 0;
               break;
         }
#ifdef GBA_LOGGING
         if (systemVerbose & VERBOSE_DMA0)
         {
            int count = (dma[0].DMACNT_L ? dma[0].DMACNT_L : 0x4000) << 1;
            if (dma[0].DMACNT_H & 0x0400)
               count <<= 1;
            log("DMA0: s=%08x d=%08x c=%04x count=%08x\n", dma[0].Source, dma[0].Dest,
                dma[0].DMACNT_H,
                count);
         }
#endif
         doDMA(dma[0].Source, dma[0].Dest, sourceIncrement, destIncrement,
             dma[0].DMACNT_L ? dma[0].DMACNT_L : 0x4000,
             dma[0].DMACNT_H & 0x0400);

         if (dma[0].DMACNT_H & 0x4000)
         {
            IF |= 0x0100;
            UPDATE_REG(REG_IF, IF);
            cpuNextEvent = cpuTotalTicks;
         }

         if (((dma[0].DMACNT_H >> 5) & 3) == 3)
         {
            dma[0].Dest = dma[0].DMADAD_L | (dma[0].DMADAD_H << 16);
         }

         if (!(dma[0].DMACNT_H & 0x0200) || (reason == 0))
         {
            dma[0].DMACNT_H &= 0x7FFF;
            UPDATE_REG(REG_DMA0CNT_H, dma[0].DMACNT_H);
         }
      }
   }

   // DMA 1
   if ((dma[1].DMACNT_H & 0x8000) && (dmamask & 2))
   {
      if (((dma[1].DMACNT_H >> 12) & 3) == reason)
      {
         uint32_t sourceIncrement = 4;
         uint32_t destIncrement = 4;
         switch ((dma[1].DMACNT_H >> 7) & 3)
         {
            case 0:
               break;
            case 1:
               sourceIncrement = (uint32_t)-4;
               break;
            case 2:
               sourceIncrement = 0;
               break;
         }
         switch ((dma[1].DMACNT_H >> 5) & 3)
         {
            case 0:
               break;
            case 1:
               destIncrement = (uint32_t)-4;
               break;
            case 2:
               destIncrement = 0;
               break;
         }
         if (reason == 3)
         {
#ifdef GBA_LOGGING
            if (systemVerbose & VERBOSE_DMA1)
            {
               log("DMA1: s=%08x d=%08x c=%04x count=%08x\n", dma[1].Source, dma[1].Dest,
                   dma[1].DMACNT_H,
                   16);
            }
#endif
            doDMA(dma[1].Source, dma[1].Dest, sourceIncrement, 0, 4,
                0x0400);
         }
         else
         {
#ifdef GBA_LOGGING
            if (systemVerbose & VERBOSE_DMA1)
            {
               int count = (dma[1].DMACNT_L ? dma[1].DMACNT_L : 0x4000) << 1;
               if (dma[1].DMACNT_H & 0x0400)
                  count <<= 1;
               log("DMA1: s=%08x d=%08x c=%04x count=%08x\n", dma[1].Source, dma[1].Dest,
                   dma[1].DMACNT_H,
                   count);
            }
#endif
            doDMA(dma[1].Source, dma[1].Dest, sourceIncrement, destIncrement,
                dma[1].DMACNT_L ? dma[1].DMACNT_L : 0x4000,
                dma[1].DMACNT_H & 0x0400);
         }

         if (dma[1].DMACNT_H & 0x4000)
         {
            IF |= 0x0200;
            UPDATE_REG(REG_IF, IF);
            cpuNextEvent = cpuTotalTicks;
         }

         if (((dma[1].DMACNT_H >> 5) & 3) == 3)
         {
            dma[1].Dest = dma[1].DMADAD_L | (dma[1].DMADAD_H << 16);
         }

         if (!(dma[1].DMACNT_H & 0x0200) || (reason == 0))
         {
            dma[1].DMACNT_H &= 0x7FFF;
            UPDATE_REG(REG_DMA1CNT_H, dma[1].DMACNT_H);
         }
      }
   }

   // DMA 2
   if ((dma[2].DMACNT_H & 0x8000) && (dmamask & 4))
   {
      if (((dma[2].DMACNT_H >> 12) & 3) == reason)
      {
         uint32_t sourceIncrement = 4;
         uint32_t destIncrement = 4;
         switch ((dma[2].DMACNT_H >> 7) & 3)
         {
            case 0:
               break;
            case 1:
               sourceIncrement = (uint32_t)-4;
               break;
            case 2:
               sourceIncrement = 0;
               break;
         }
         switch ((dma[2].DMACNT_H >> 5) & 3)
         {
            case 0:
               break;
            case 1:
               destIncrement = (uint32_t)-4;
               break;
            case 2:
               destIncrement = 0;
               break;
         }
         if (reason == 3)
         {
#ifdef GBA_LOGGING
            if (systemVerbose & VERBOSE_DMA2)
            {
               int count = (4) << 2;
               log("DMA2: s=%08x d=%08x c=%04x count=%08x\n", dma[2].Source, dma[2].Dest,
                   dma[2].DMACNT_H,
                   count);
            }
#endif
            doDMA(dma[2].Source, dma[2].Dest, sourceIncrement, 0, 4,
                0x0400);
         }
         else
         {
#ifdef GBA_LOGGING
            if (systemVerbose & VERBOSE_DMA2)
            {
               int count = (dma[2].DMACNT_L ? dma[2].DMACNT_L : 0x4000) << 1;
               if (dma[2].DMACNT_H & 0x0400)
                  count <<= 1;
               log("DMA2: s=%08x d=%08x c=%04x count=%08x\n", dma[2].Source, dma[2].Dest,
                   dma[2].DMACNT_H,
                   count);
            }
#endif
            doDMA(dma[2].Source, dma[2].Dest, sourceIncrement, destIncrement,
                dma[2].DMACNT_L ? dma[2].DMACNT_L : 0x4000,
                dma[2].DMACNT_H & 0x0400);
         }

         if (dma[2].DMACNT_H & 0x4000)
         {
            IF |= 0x0400;
            UPDATE_REG(REG_IF, IF);
            cpuNextEvent = cpuTotalTicks;
         }

         if (((dma[2].DMACNT_H >> 5) & 3) == 3)
         {
            dma[2].Dest = dma[2].DMADAD_L | (dma[2].DMADAD_H << 16);
         }

         if (!(dma[2].DMACNT_H & 0x0200) || (reason == 0))
         {
            dma[2].DMACNT_H &= 0x7FFF;
            UPDATE_REG(REG_DMA2CNT_H, dma[2].DMACNT_H);
         }
      }
   }

   // DMA 3
   if ((dma[3].DMACNT_H & 0x8000) && (dmamask & 8))
   {
      if (((dma[3].DMACNT_H >> 12) & 3) == reason)
      {
         uint32_t sourceIncrement = 4;
         uint32_t destIncrement = 4;
         switch ((dma[3].DMACNT_H >> 7) & 3)
         {
            case 0:
               break;
            case 1:
               sourceIncrement = (uint32_t)-4;
               break;
            case 2:
               sourceIncrement = 0;
               break;
         }
         switch ((dma[3].DMACNT_H >> 5) & 3)
         {
            case 0:
               break;
            case 1:
               destIncrement = (uint32_t)-4;
               break;
            case 2:
               destIncrement = 0;
               break;
         }
#ifdef GBA_LOGGING
         if (systemVerbose & VERBOSE_DMA3)
         {
            int count = (dma[3].DMACNT_L ? dma[3].DMACNT_L : 0x10000) << 1;
            if (dma[3].DMACNT_H & 0x0400)
               count <<= 1;
            log("DMA3: s=%08x d=%08x c=%04x count=%08x\n", dma[3].Source, dma[3].Dest,
                dma[3].DMACNT_H,
                count);
         }
#endif
         doDMA(dma[3].Source, dma[3].Dest, sourceIncrement, destIncrement,
             dma[3].DMACNT_L ? dma[3].DMACNT_L : 0x10000,
             dma[3].DMACNT_H & 0x0400);

         if (dma[3].DMACNT_H & 0x4000)
         {
            IF |= 0x0800;
            UPDATE_REG(REG_IF, IF);
            cpuNextEvent = cpuTotalTicks;
         }

         if (((dma[3].DMACNT_H >> 5) & 3) == 3)
         {
            dma[3].Dest = dma[3].DMADAD_L | (dma[3].DMADAD_H << 16);
         }

         if (!(dma[3].DMACNT_H & 0x0200) || (reason == 0))
         {
            dma[3].DMACNT_H &= 0x7FFF;
            UPDATE_REG(REG_DMA3CNT_H, dma[3].DMACNT_H);
         }
      }
   }
}

void applyTimer()
{
   if (timers.OnOffDelay & 1)
   {
      timers.tm[0].ClockReload = TIMER_TICKS[timers.tm[0].Value & 3];
      if (!timers.tm[0].On && (timers.tm[0].Value & 0x80))
      {
         // reload the counter
         timers.tm[0].TMCNT_L = timers.tm[0].Reload;
         timers.tm[0].Ticks = (0x10000 - timers.tm[0].TMCNT_L) << timers.tm[0].ClockReload;
         UPDATE_REG(REG_TM0CNT_L, timers.tm[0].TMCNT_L);
      }
      timers.tm[0].On = timers.tm[0].Value & 0x80 ? true : false;
      timers.tm[0].TMCNT_H = timers.tm[0].Value & 0xC7;
      interp_rate();
      UPDATE_REG(REG_TM0CNT_H, timers.tm[0].TMCNT_H);
      //    CPUUpdateTicks();
   }
   if (timers.OnOffDelay & 2)
   {
      timers.tm[1].ClockReload = TIMER_TICKS[timers.tm[1].Value & 3];
      if (!timers.tm[1].On && (timers.tm[1].Value & 0x80))
      {
         // reload the counter
         timers.tm[1].TMCNT_L = timers.tm[1].Reload;
         timers.tm[1].Ticks = (0x10000 - timers.tm[1].TMCNT_L) << timers.tm[1].ClockReload;
         UPDATE_REG(REG_TM1CNT_L, timers.tm[1].TMCNT_L);
      }
      timers.tm[1].On = timers.tm[1].Value & 0x80 ? true : false;
      timers.tm[1].TMCNT_H = timers.tm[1].Value & 0xC7;
      interp_rate();
      UPDATE_REG(REG_TM1CNT_H, timers.tm[1].TMCNT_H);
   }
   if (timers.OnOffDelay & 4)
   {
      timers.tm[2].ClockReload = TIMER_TICKS[timers.tm[2].Value & 3];
      if (!timers.tm[2].On && (timers.tm[2].Value & 0x80))
      {
         // reload the counter
         timers.tm[2].TMCNT_L = timers.tm[2].Reload;
         timers.tm[2].Ticks = (0x10000 - timers.tm[2].TMCNT_L) << timers.tm[2].ClockReload;
         UPDATE_REG(REG_TM2CNT_L, timers.tm[2].TMCNT_L);
      }
      timers.tm[2].On = timers.tm[2].Value & 0x80 ? true : false;
      timers.tm[2].TMCNT_H = timers.tm[2].Value & 0xC7;
      UPDATE_REG(REG_TM2CNT_H, timers.tm[2].TMCNT_H);
   }
   if (timers.OnOffDelay & 8)
   {
      timers.tm[3].ClockReload = TIMER_TICKS[timers.tm[3].Value & 3];
      if (!timers.tm[3].On && (timers.tm[3].Value & 0x80))
      {
         // reload the counter
         timers.tm[3].TMCNT_L = timers.tm[3].Reload;
         timers.tm[3].Ticks = (0x10000 - timers.tm[3].TMCNT_L) << timers.tm[3].ClockReload;
         UPDATE_REG(REG_TM3CNT_L, timers.tm[3].TMCNT_L);
      }
      timers.tm[3].On = timers.tm[3].Value & 0x80 ? true : false;
      timers.tm[3].TMCNT_H = timers.tm[3].Value & 0xC7;
      UPDATE_REG(REG_TM3CNT_H, timers.tm[3].TMCNT_H);
   }
   cpuNextEvent = CPUUpdateTicks();
   timers.OnOffDelay = 0;
}

uint8_t cpuBitsSet[256];

void CPUInit(const char* biosFileName, bool useBiosFile)
{
#ifdef MSB_FIRST
   if (!cpuBiosSwapped)
   {
      for (unsigned int i = 0; i < sizeof(myROM) / 4; i++)
      {
         WRITE32LE(&myROM[i], myROM[i]);
      }
      cpuBiosSwapped = true;
   }
#endif
   eepromInUse = 0;
   useBios = false;

   if (useBiosFile && strlen(biosFileName) > 0)
   {
      int size = 0x4000;
      if (utilLoad(biosFileName,
              CPUIsGBABios,
              bios,
              size))
      {
         if (size == 0x4000)
            useBios = true;
         else
            systemMessage(MSG_INVALID_BIOS_FILE_SIZE, N_("Invalid BIOS file size"));
      }
   }

   if (!useBios)
   {
      memcpy(bios, myROM, sizeof(myROM));
   }

   int i = 0;

   biosProtected[0] = 0x00;
   biosProtected[1] = 0xf0;
   biosProtected[2] = 0x29;
   biosProtected[3] = 0xe1;

   for (i = 0; i < 256; i++)
   {
      int count = 0;
      int j;
      for (j = 0; j < 8; j++)
         if (i & (1 << j))
            count++;
      cpuBitsSet[i] = count;

      for (j = 0; j < 8; j++)
         if (i & (1 << j))
            break;
   }

   for (i = 0; i < 0x400; i++)
      ioReadable[i] = true;
   for (i = 0x10; i < 0x48; i++)
      ioReadable[i] = false;
   for (i = 0x4c; i < 0x50; i++)
      ioReadable[i] = false;
   for (i = 0x54; i < 0x60; i++)
      ioReadable[i] = false;
   for (i = 0x8c; i < 0x90; i++)
      ioReadable[i] = false;
   for (i = 0xa0; i < 0xb8; i++)
      ioReadable[i] = false;
   for (i = 0xbc; i < 0xc4; i++)
      ioReadable[i] = false;
   for (i = 0xc8; i < 0xd0; i++)
      ioReadable[i] = false;
   for (i = 0xd4; i < 0xdc; i++)
      ioReadable[i] = false;
   for (i = 0xe0; i < 0x100; i++)
      ioReadable[i] = false;
   for (i = 0x110; i < 0x120; i++)
      ioReadable[i] = false;
   for (i = 0x12c; i < 0x130; i++)
      ioReadable[i] = false;
   for (i = 0x138; i < 0x140; i++)
      ioReadable[i] = false;
   for (i = 0x144; i < 0x150; i++)
      ioReadable[i] = false;
   for (i = 0x15c; i < 0x200; i++)
      ioReadable[i] = false;
   for (i = 0x20c; i < 0x300; i++)
      ioReadable[i] = false;
   for (i = 0x304; i < 0x400; i++)
      ioReadable[i] = false;
   
   if (romSize < 0x1fe2000)
   {
      *((uint16_t*)&rom[0x1fe209c]) = 0xdffa; // SWI 0xFA
      *((uint16_t*)&rom[0x1fe209e]) = 0x4770; // BX LR
   }
   else
   {
      //agbPrintEnable(false);
   }
}

void SetSaveType(int st)
{
   switch (st)
   {
      case GBA_SAVE_AUTO:
         cpuSramEnabled = true;
         cpuFlashEnabled = true;
         cpuEEPROMEnabled = true;
         cpuEEPROMSensorEnabled = false;
         cpuSaveGameFunc = flashSaveDecide;
         break;
      case GBA_SAVE_EEPROM:
         cpuSramEnabled = false;
         cpuFlashEnabled = false;
         cpuEEPROMEnabled = true;
         cpuEEPROMSensorEnabled = false;
         break;
      case GBA_SAVE_SRAM:
         cpuSramEnabled = true;
         cpuFlashEnabled = false;
         cpuEEPROMEnabled = false;
         cpuEEPROMSensorEnabled = false;
         cpuSaveGameFunc = sramDelayedWrite; // to insure we detect the write
         break;
      case GBA_SAVE_FLASH:
         cpuSramEnabled = false;
         cpuFlashEnabled = true;
         cpuEEPROMEnabled = false;
         cpuEEPROMSensorEnabled = false;
         cpuSaveGameFunc = flashDelayedWrite; // to insure we detect the write
         break;
      case GBA_SAVE_EEPROM_SENSOR:
         cpuSramEnabled = false;
         cpuFlashEnabled = false;
         cpuEEPROMEnabled = true;
         cpuEEPROMSensorEnabled = true;
         break;
      case GBA_SAVE_NONE:
         cpuSramEnabled = false;
         cpuFlashEnabled = false;
         cpuEEPROMEnabled = false;
         cpuEEPROMSensorEnabled = false;
         break;
   }
}

void CPUReset()
{
   /*switch (CheckEReaderRegion()) {
    case 1: //US
        EReaderWriteMemory(0x8009134, 0x46C0DFE0);
        break;
    case 2:
        EReaderWriteMemory(0x8008A8C, 0x46C0DFE0);
        break;
    case 3:
        EReaderWriteMemory(0x80091A8, 0x46C0DFE0);
        break;
    }*/
   rtcReset();
   // clean registers
   memset(&reg[0], 0, sizeof(reg));
   // clean OAM
   memset(oam, 0, SIZE_OAM);
   // clean palette
   memset(paletteRAM, 0, SIZE_PRAM);
   // clean picture
   memset(pix, 0, SIZE_PIX);
   // clean vram
   memset(vram, 0, SIZE_VRAM);
   // clean io memory
   memset(ioMem, 0, SIZE_IOMEM);

   renderer.mode = 0;
   renderer.type = 0;
   fxOn = false;
   windowOn = false;

   // Reset IO Registers
   lcd.dispcnt = 0x0080;
   lcd.vcount = (useBios && !skipBios) ? 0 : 0x007E;
   LCDResetBGRegisters();
   for (int i = 0; i < 4; i++)
   {
      dma[i].DMASAD_L = 0x0000;
      dma[i].DMASAD_H = 0x0000;
      dma[i].DMADAD_L = 0x0000;
      dma[i].DMADAD_H = 0x0000;
      dma[i].DMACNT_L = 0x0000;
      dma[i].DMACNT_H = 0x0000;
   }
   for (int i = 0; i < 4; i++)
   {
      timers.tm[i].TMCNT_L = 0x0000;
      timers.tm[i].TMCNT_H = 0x0000;
   }
   P1 = 0x03FF;
   IE = 0x0000;
   IF = 0x0000;
   IME = 0x0000;

   armMode = 0x1F;

   if (cpuIsMultiBoot)
   {
      reg[13].I = 0x03007F00;
      reg[15].I = 0x02000000;
      reg[16].I = 0x00000000;
      reg[R13_IRQ].I = 0x03007FA0;
      reg[R13_SVC].I = 0x03007FE0;
      armIrqEnable = true;
   }
   else
   {
      if (useBios && !skipBios)
      {
         reg[15].I = 0x00000000;
         armMode = 0x13;
         armIrqEnable = false;
      }
      else
      {
         reg[13].I = 0x03007F00;
         reg[15].I = 0x08000000;
         reg[16].I = 0x00000000;
         reg[R13_IRQ].I = 0x03007FA0;
         reg[R13_SVC].I = 0x03007FE0;
         armIrqEnable = true;
      }
   }
   armState = true;
   C_FLAG = V_FLAG = N_FLAG = Z_FLAG = false;
   UPDATE_REG(REG_DISPCNT, lcd.dispcnt);
   UPDATE_REG(REG_VCOUNT, lcd.vcount);
   UPDATE_REG(REG_BG2PA, lcd.bg[2].dx);
   UPDATE_REG(REG_BG2PD, lcd.bg[2].dmy);
   UPDATE_REG(REG_BG3PA, lcd.bg[3].dy);
   UPDATE_REG(REG_BG3PD, lcd.bg[3].dmy);
   UPDATE_REG(REG_KEYINPUT, P1);
   UPDATE_REG(REG_SOUNDBIAS, 0x200);

   // disable FIQ
   reg[16].I |= 0x40;

   CPUUpdateCPSR();

   armNextPC = reg[15].I;
   reg[15].I += 4;

   // reset internal state
   holdState = false;
   holdType = 0;

   biosProtected[0] = 0x00;
   biosProtected[1] = 0xf0;
   biosProtected[2] = 0x29;
   biosProtected[3] = 0xe1;

   for (int i = 0; i < 4; i++)
   {
      timers.tm[i].On = false;
      timers.tm[i].Ticks = 0;
      timers.tm[i].Reload = 0;
      timers.tm[i].ClockReload = 0;
   }

   for (int i = 0; i < 4; i++)
   {
      dma[i].Source = 0;
      dma[i].Dest = 0;
   }

   lcdTicks = (useBios && !skipBios) ? 1008 : 208;
   layerEnable = lcd.dispcnt & layerSettings;

   CPUUpdateRenderBuffers(true);

   for (int i = 0; i < 256; i++)
   {
      map[i].address = (uint8_t*)&dummyAddress;
      map[i].mask = 0;
   }

   map[0].address = bios;
   map[2].address = workRAM;
   map[3].address = internalRAM;
   map[4].address = ioMem;
   map[5].address = paletteRAM;
   map[6].address = vram;
   map[7].address = oam;
   map[8].address = rom;
   map[9].address = rom;
   map[10].address = rom;
   map[12].address = rom;
   map[14].address = flashSaveMemory;

   SetMapMasks();

   soundReset();

   LCDUpdateWindow0();
   LCDUpdateWindow1();

   // make sure registers are correctly initialized if not using BIOS
   if (!useBios)
   {
      if (cpuIsMultiBoot)
         BIOS_RegisterRamReset(0xfe);
      else
         BIOS_RegisterRamReset(0xff);
   }
   else
   {
      if (cpuIsMultiBoot)
         BIOS_RegisterRamReset(0xfe);
   }

   flashReset();
   eepromReset();
   SetSaveType(saveType);

   ARM_PREFETCH;

   systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

   cpuDmaHack = false;

   SWITicks = 0;
}

void CPUInterrupt()
{
   uint32_t PC = reg[15].I;
   bool savedState = armState;
   CPUSwitchMode(0x12, true, false);
   reg[14].I = PC;
   if (!savedState)
      reg[14].I += 2;
   reg[15].I = 0x18;
   armState = true;
   armIrqEnable = false;

   armNextPC = reg[15].I;
   reg[15].I += 4;
   ARM_PREFETCH;
   WRITE32LE((uint32_t*)&biosProtected, 0xe3a02004);
}

static uint16_t joy;
static bool hasFrames;

static void gbaUpdateJoypads(uint16_t *joybuf)
{
   // update joystick information
   if (systemReadJoypads())
      // read default joystick
      joy = joybuf[0];

   P1 = 0x03FF ^ (joy & 0x3FF);
   UPDATE_REG(REG_KEYINPUT, P1);
   uint16_t P1CNT = READ16LE(((uint16_t*)&ioMem[REG_KEYCNT]));

   // this seems wrong, but there are cases where the game
   // can enter the stop state without requesting an IRQ from
   // the joypad.
   if ((P1CNT & 0x4000) || stopState)
   {
      uint16_t p1 = (0x3FF ^ P1) & 0x3FF;
      if (P1CNT & 0x8000)
      {
         if (p1 == (P1CNT & 0x3FF))
         {
            IF |= 0x1000;
            UPDATE_REG(REG_IF, IF);
         }
      }
      else
      {
         if (p1 & P1CNT)
         {
            IF |= 0x1000;
            UPDATE_REG(REG_IF, IF);
         }
      }
   }

   systemUpdateMotionSensor();
}

void CPULoop(int ticks, uint16_t *joybuf)
{
   int clockTicks;
   int timerOverflow = 0;
   // variable used by the CPU core
   cpuTotalTicks = 0;

#ifndef NO_LINK
// shuffle2: what's the purpose?
//if(GetLinkMode() != LINK_DISCONNECTED)
//cpuNextEvent = 1;
#endif

   hasFrames = false;
   cpuBreakLoop = false;
   cpuNextEvent = CPUUpdateTicks();
   if (cpuNextEvent > ticks)
      cpuNextEvent = ticks;

   gbaUpdateJoypads(joybuf);

   for (;;)
   {
      if (!holdState && !SWITicks)
      {
         if (armState)
         {
            if (!armExecute())
               return;
         }
         else
         {
            if (!thumbExecute())
               return;
         }
         clockTicks = 0;
      }
      else
         clockTicks = CPUUpdateTicks();

      cpuTotalTicks += clockTicks;

      if (rtcIsEnabled())
         rtcUpdateTime(cpuTotalTicks);

      if (cpuTotalTicks >= cpuNextEvent)
      {
         int remainingTicks = cpuTotalTicks - cpuNextEvent;

         if (SWITicks)
         {
            SWITicks -= clockTicks;
            if (SWITicks < 0)
               SWITicks = 0;
         }

         clockTicks = cpuNextEvent;
         cpuTotalTicks = 0;

      updateLoop:

         if (IRQTicks)
         {
            IRQTicks -= clockTicks;
            if (IRQTicks < 0)
               IRQTicks = 0;
         }

         lcdTicks -= clockTicks;

         soundUpdateTicks(clockTicks);

         if (lcdTicks <= 0)
         {
            if (lcd.dispstat & 1)
            {
               // V-BLANK
               // if in V-Blank mode, keep computing...
               if (lcd.dispstat & 2)
               {
                  lcdTicks += 1008;
                  lcd.vcount++;
                  UPDATE_REG(REG_VCOUNT, lcd.vcount);
                  lcd.dispstat &= 0xFFFD;
                  UPDATE_REG(REG_DISPSTAT, lcd.dispstat);
                  CPUCompareVCOUNT();
               }
               else
               {
                  lcdTicks += 224;
                  lcd.dispstat |= 2;
                  UPDATE_REG(REG_DISPSTAT, lcd.dispstat);
                  if (lcd.dispstat & 16)
                  {
                     IF |= 2;
                     UPDATE_REG(REG_IF, IF);
                  }
               }
               if (lcd.vcount > 227)
               {
                  //Reaching last line
                  lcd.dispstat &= 0xFFFC;
                  UPDATE_REG(REG_DISPSTAT, lcd.dispstat);
                  lcd.vcount = 0;
                  UPDATE_REG(REG_VCOUNT, lcd.vcount);
                  CPUCompareVCOUNT();
               }
            }
            else
            {
               if (lcd.dispstat & 2)
               {
                  // if in H-Blank, leave it and move to drawing mode
                  lcd.vcount++;
                  UPDATE_REG(REG_VCOUNT, lcd.vcount);
                  lcdTicks += 1008;
                  lcd.dispstat &= 0xFFFD;
                  if (lcd.vcount == 160)
                  {
                     //uint32_t ext = (joy >> 10);
                     // If no (m) code is enabled, apply the cheats at each LCDline
                     //if ((cheatsEnabled) && (mastercode == 0)) {
                     //remainingTicks += cheatsCheckKeys(P1 ^ 0x3FF, ext);
                     //}
                     lcd.dispstat |= 1;
                     lcd.dispstat &= 0xFFFD;
                     UPDATE_REG(REG_DISPSTAT, lcd.dispstat);
                     if (lcd.dispstat & 0x0008)
                     {
                        IF |= 1;
                        UPDATE_REG(REG_IF, IF);
                     }
                     CPUCheckDMA(1, 0x0f);
                     systemFrame();
                     systemDrawScreen(pix);
                     hasFrames = true;
                  }
                  UPDATE_REG(REG_DISPSTAT, lcd.dispstat);
                  CPUCompareVCOUNT();
               }
               else
               {
                  // scanline drawing mode
                  //if ((lcd.vcount < 160) /* && (frameCount >= framesToSkip) */)
                  {
                     pixFormat* dest = pix + GBA_WIDTH * lcd.vcount;
                     gfxDrawScanline(dest, renderer.mode, renderer.type);
                  }
                  // entering H-Blank
                  lcd.dispstat |= 2;
                  UPDATE_REG(REG_DISPSTAT, lcd.dispstat);
                  lcdTicks += 224;
                  CPUCheckDMA(2, 0x0f);
                  if (lcd.dispstat & 16)
                  {
                     IF |= 2;
                     UPDATE_REG(REG_IF, IF);
                  }
               }
            }
         }

         if (!stopState)
         {
            if (timers.tm[0].On)
            {
               timers.tm[0].Ticks -= clockTicks;
               if (timers.tm[0].Ticks <= 0)
               {
                  timers.tm[0].Ticks += (0x10000 - timers.tm[0].Reload) << timers.tm[0].ClockReload;
                  timerOverflow |= 1;
                  soundTimerOverflow(0);
                  if (timers.tm[0].TMCNT_H & 0x40)
                  {
                     IF |= 0x08;
                     UPDATE_REG(REG_IF, IF);
                  }
               }
               timers.tm[0].TMCNT_L = 0xFFFF - (timers.tm[0].Ticks >> timers.tm[0].ClockReload);
               UPDATE_REG(REG_TM0CNT_L, timers.tm[0].TMCNT_L);
            }

            if (timers.tm[1].On)
            {
               if (timers.tm[1].TMCNT_H & 4)
               {
                  if (timerOverflow & 1)
                  {
                     timers.tm[1].TMCNT_L++;
                     if (timers.tm[1].TMCNT_L == 0)
                     {
                        timers.tm[1].TMCNT_L += timers.tm[1].Reload;
                        timerOverflow |= 2;
                        soundTimerOverflow(1);
                        if (timers.tm[1].TMCNT_H & 0x40)
                        {
                           IF |= 0x10;
                           UPDATE_REG(REG_IF, IF);
                        }
                     }
                     UPDATE_REG(REG_TM1CNT_L, timers.tm[1].TMCNT_L);
                  }
               }
               else
               {
                  timers.tm[1].Ticks -= clockTicks;
                  if (timers.tm[1].Ticks <= 0)
                  {
                     timers.tm[1].Ticks += (0x10000 - timers.tm[1].Reload) << timers.tm[1].ClockReload;
                     timerOverflow |= 2;
                     soundTimerOverflow(1);
                     if (timers.tm[1].TMCNT_H & 0x40)
                     {
                        IF |= 0x10;
                        UPDATE_REG(REG_IF, IF);
                     }
                  }
                  timers.tm[1].TMCNT_L = 0xFFFF - (timers.tm[1].Ticks >> timers.tm[1].ClockReload);
                  UPDATE_REG(REG_TM1CNT_L, timers.tm[1].TMCNT_L);
               }
            }

            if (timers.tm[2].On)
            {
               if (timers.tm[2].TMCNT_H & 4)
               {
                  if (timerOverflow & 2)
                  {
                     timers.tm[2].TMCNT_L++;
                     if (timers.tm[2].TMCNT_L == 0)
                     {
                        timers.tm[2].TMCNT_L += timers.tm[2].Reload;
                        timerOverflow |= 4;
                        if (timers.tm[2].TMCNT_H & 0x40)
                        {
                           IF |= 0x20;
                           UPDATE_REG(REG_IF, IF);
                        }
                     }
                     UPDATE_REG(REG_TM2CNT_L, timers.tm[2].TMCNT_L);
                  }
               }
               else
               {
                  timers.tm[2].Ticks -= clockTicks;
                  if (timers.tm[2].Ticks <= 0)
                  {
                     timers.tm[2].Ticks += (0x10000 - timers.tm[2].Reload) << timers.tm[2].ClockReload;
                     timerOverflow |= 4;
                     if (timers.tm[2].TMCNT_H & 0x40)
                     {
                        IF |= 0x20;
                        UPDATE_REG(REG_IF, IF);
                     }
                  }
                  timers.tm[2].TMCNT_L = 0xFFFF - (timers.tm[2].Ticks >> timers.tm[2].ClockReload);
                  UPDATE_REG(REG_TM2CNT_L, timers.tm[2].TMCNT_L);
               }
            }

            if (timers.tm[3].On)
            {
               if (timers.tm[3].TMCNT_H & 4)
               {
                  if (timerOverflow & 4)
                  {
                     timers.tm[3].TMCNT_L++;
                     if (timers.tm[3].TMCNT_L == 0)
                     {
                        timers.tm[3].TMCNT_L += timers.tm[3].Reload;
                        if (timers.tm[3].TMCNT_H & 0x40)
                        {
                           IF |= 0x40;
                           UPDATE_REG(REG_IF, IF);
                        }
                     }
                     UPDATE_REG(REG_TM3CNT_L, timers.tm[3].TMCNT_L);
                  }
               }
               else
               {
                  timers.tm[3].Ticks -= clockTicks;
                  if (timers.tm[3].Ticks <= 0)
                  {
                     timers.tm[3].Ticks += (0x10000 - timers.tm[3].Reload) << timers.tm[3].ClockReload;
                     if (timers.tm[3].TMCNT_H & 0x40)
                     {
                        IF |= 0x40;
                        UPDATE_REG(REG_IF, IF);
                     }
                  }
                  timers.tm[3].TMCNT_L = 0xFFFF - (timers.tm[3].Ticks >> timers.tm[3].ClockReload);
                  UPDATE_REG(REG_TM3CNT_L, timers.tm[3].TMCNT_L);
               }
            }
         }

         timerOverflow = 0;

         ticks -= clockTicks;

#ifndef NO_LINK
         if (GetLinkMode() != LINK_DISCONNECTED)
            LinkUpdate(clockTicks);
#endif

         cpuNextEvent = CPUUpdateTicks();

         if (cpuDmaTicksToUpdate > 0)
         {
            if (cpuDmaTicksToUpdate > cpuNextEvent)
               clockTicks = cpuNextEvent;
            else
               clockTicks = cpuDmaTicksToUpdate;
            cpuDmaTicksToUpdate -= clockTicks;
            if (cpuDmaTicksToUpdate < 0)
               cpuDmaTicksToUpdate = 0;
            goto updateLoop;
         }

#ifndef NO_LINK
         // shuffle2: what's the purpose?
         if (GetLinkMode() != LINK_DISCONNECTED || gba_joybus_active)
            cpuNextEvent = 1;
#endif

         if (IF && (IME & 1) && armIrqEnable)
         {
            int res = IF & IE;
            if (stopState)
               res &= 0x3080;
            if (res)
            {
               if (intState)
               {
                  if (!IRQTicks)
                  {
                     CPUInterrupt();
                     intState = false;
                     holdState = false;
                     stopState = false;
                     holdType = 0;
                  }
               }
               else
               {
                  if (!holdState)
                  {
                     intState = true;
                     IRQTicks = 7;
                     if (cpuNextEvent > IRQTicks)
                        cpuNextEvent = IRQTicks;
                  }
                  else
                  {
                     CPUInterrupt();
                     holdState = false;
                     stopState = false;
                     holdType = 0;
                  }
               }

               // Stops the SWI Ticks emulation if an IRQ is executed
               //(to avoid problems with nested IRQ/SWI)
               if (SWITicks)
                  SWITicks = 0;
            }
         }

         if (remainingTicks > 0)
         {
            if (remainingTicks > cpuNextEvent)
               clockTicks = cpuNextEvent;
            else
               clockTicks = remainingTicks;
            remainingTicks -= clockTicks;
            if (remainingTicks < 0)
               remainingTicks = 0;
            goto updateLoop;
         }

         if (timers.OnOffDelay)
            applyTimer();

         if (cpuNextEvent > ticks)
            cpuNextEvent = ticks;

         // end loop when a frame is done
         if (ticks <= 0 || cpuBreakLoop || hasFrames)
            break;
      }
   }
#ifndef NO_LINK
   if (GetLinkMode() != LINK_DISCONNECTED)
      CheckLinkConnection();
#endif
}

struct EmulatedSystem GBASystem = {
   // emuMain
   CPULoop,
   // emuReset
   CPUReset,
   // emuCleanUp
   CPUCleanUp,
   // emuReadBattery
   NULL,
   // emuWriteBattery
   NULL,
   // emuReadState
   CPUReadState,
   // emuWriteState
   CPUWriteState,
   // emuReadMemState
   NULL,
   // emuWriteMemState
   NULL,
   // emuWritePNG
   NULL,
   // emuWriteBMP
   NULL,
   // emuUpdateCPSR
   CPUUpdateCPSR,
   // emuFlushAudio
   soundEndFrame,
   // emuHasDebugger
   true,
// emuCount
#ifdef FINAL_VERSION
   300000
#else
   5000
#endif
};
