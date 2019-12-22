#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libretro.h"
#include "SoundRetro.h"
#include "libretro_input.h"
#include "libretro_core_options.h"
#include "scrc32.h"

#include "../System.h"
#include "../Util.h"
#include "../apu/Blip_Buffer.h"
#include "../apu/Gb_Apu.h"
#include "../apu/Gb_Oscs.h"

#include "../common/ConfigManager.h"
#include "../common/Port.h"

#include "../gba/Cheats.h"
#include "../gba/EEprom.h"
#include "../gba/Flash.h"
#include "../gba/GBA.h"
#include "../gba/GBAGfx.h"
#include "../gba/Globals.h"
#include "../gba/RTC.h"
#include "../gba/Sound.h"
#include "../gba/bios.h"

#include "../gb/gb.h"
#include "../gb/gbCheats.h"
#include "../gb/gbGlobals.h"
#include "../gb/gbMemory.h"
#include "../gb/gbSGB.h"
#include "../gb/gbSound.h"

#define FRAMERATE  (16777216.0 / 280896.0) // 59.73
#define SAMPLERATE 32768.0

// just provide enough audio buffer size
#define SOUND_BUFFER_SIZE 4096

static retro_log_printf_t log_cb;
static retro_video_refresh_t video_cb;
static retro_input_poll_t poll_cb;
static retro_input_state_t input_cb;
static retro_environment_t environ_cb;
retro_audio_sample_batch_t audio_batch_cb;

static char retro_system_directory[2048];
static char biosfile[4096];

// api settings
static bool can_dupe = false;
static bool libretro_supports_bitmasks = false;

// core options
static bool option_useBios             = false;
static bool option_colorizerHack       = false;
static bool option_forceRTCenable      = false;
static bool option_showAdvancedOptions = false;
static unsigned option_gbPalette       = 0;
static double option_sndFiltering      = 0.5;
static bool option_sndInterpolation    = true;
bool option_turboEnable                = false;
unsigned option_turboDelay             = 3;
int option_analogDeadzone              = 0;
int option_gyroSensitivity             = 0;
int option_tiltSensitivity             = 0;
bool option_swapAnalogSticks           = false;

static unsigned systemWidth  = GBA_WIDTH;
static unsigned systemHeight = GBA_HEIGHT;
static EmulatedSystem *core  = NULL;

uint16_t input_buf[4]  = { 0 };
unsigned input_type[4] = { 0 };

// global vars
uint16_t systemColorMap16[0x10000];
uint32_t systemColorMap32[0x10000];
int RGB_LOW_BITS_MASK = 0;
int systemRedShift = 0;
int systemBlueShift = 0;
int systemGreenShift = 0;
int systemColorDepth = 32;
int systemDebug = 0;
int systemVerbose = 0;
int systemFrameSkip = 0;
int systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
int systemSpeed = 0;
int emulating = 0;
int romSize = 0;
int hardware = HW_NONE;

void (*dbgOutput)(const char *s, uint32_t addr);
void (*dbgSignal)(int sig, int number);

#ifdef _WIN32
static const char SLASH = '\\';
#else
static const char SLASH = '/';
#endif

#define GS555(x) (x | (x << 5) | (x << 10))
uint16_t systemGbPalette[24];

struct palettes_t
{
   const char *name;
   uint16_t data[8];
};

static palettes_t defaultGBPalettes[] = {
   {
       "Standard",
       { 0x7FFF, 0x56B5, 0x318C, 0x0000, 0x7FFF, 0x56B5, 0x318C, 0x0000 },
   },
   {
       "Blue Sea",
       { 0x6200, 0x7E10, 0x7C10, 0x5000, 0x6200, 0x7E10, 0x7C10, 0x5000 },
   },
   {
       "Dark Night",
       { 0x4008, 0x4000, 0x2000, 0x2008, 0x4008, 0x4000, 0x2000, 0x2008 },
   },
   {
       "Green Forest",
       { 0x43F0, 0x03E0, 0x4200, 0x2200, 0x43F0, 0x03E0, 0x4200, 0x2200 },
   },
   {
       "Hot Desert",
       { 0x43FF, 0x03FF, 0x221F, 0x021F, 0x43FF, 0x03FF, 0x221F, 0x021F },
   },
   {
       "Pink Dreams",
       { 0x621F, 0x7E1F, 0x7C1F, 0x2010, 0x621F, 0x7E1F, 0x7C1F, 0x2010 },
   },
   {
       "Weird Colors",
       { 0x621F, 0x401F, 0x001F, 0x2010, 0x621F, 0x401F, 0x001F, 0x2010 },
   },
   {
       "Real GB Colors",
       { 0x1B8E, 0x02C0, 0x0DA0, 0x1140, 0x1B8E, 0x02C0, 0x0DA0, 0x1140 },
   },
   {
       "Real GB on GBASP Colors",
       { 0x7BDE, 0x5778, 0x5640, 0x0000, 0x7BDE, 0x529C, 0x2990, 0x0000 },
   }
};

static void set_gbPalette(void)
{
   if (core->type != IMAGE_GB)
      return;

   if (gbCgbMode || gbSgbMode)
      return;

   const uint16_t *pal = defaultGBPalettes[option_gbPalette].data;
   for (int i = 0; i < 8; i++)
   {
      uint16_t val = pal[i];
      gbPalette[i] = val;
   }
}

static void gbInitRTC(void)
{
   struct tm *lt;
   time_t rawtime;
   time(&rawtime);
   lt = localtime(&rawtime);

   switch (gbRomType)
   {
   case 0x0f:
   case 0x10:
      rtcData.mapperSeconds = lt->tm_sec;
      rtcData.mapperMinutes = lt->tm_min;
      rtcData.mapperHours = lt->tm_hour;
      rtcData.mapperDays = lt->tm_yday & 255;
      rtcData.mapperControl = (rtcData.mapperControl & 0xfe) | (lt->tm_yday > 255 ? 1 : 0);
      rtcData.mapperLastTime = rawtime;
      break;
   case 0xfd:
   {
      uint8_t gbDaysinMonth[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
      int days = lt->tm_yday + 365 * 3;
      rtcData.mapperSeconds = lt->tm_sec;
      rtcData.mapperMinutes = lt->tm_min;
      rtcData.mapperHours = lt->tm_hour;
      rtcData.mapperDays = 1;
      rtcData.mapperMonths = 1;
      rtcData.mapperYears = 1970;
      rtcData.mapperLastTime = rawtime;
      while (days)
      {
         rtcData.mapperDays++;
         days--;
         if (rtcData.mapperDays > gbDaysinMonth[rtcData.mapperMonths - 1])
         {
            rtcData.mapperDays = 1;
            rtcData.mapperMonths++;
            if (rtcData.mapperMonths > 12)
            {
               rtcData.mapperMonths = 1;
               rtcData.mapperYears++;
               if ((rtcData.mapperYears & 3) == 0)
                  gbDaysinMonth[1] = 29;
               else
                  gbDaysinMonth[1] = 28;
            }
         }
      }
      rtcData.mapperControl = (rtcData.mapperControl & 0xfe) | (lt->tm_yday > 255 ? 1 : 0);
   }
   break;
   }
}

static void SetGBBorder(unsigned val)
{
   struct retro_system_av_info avinfo = { 0 };
   unsigned geometry_changed = 0;

   switch (val)
   {
   case 0:
      geometry_changed = ((systemWidth != GB_WIDTH) || (systemHeight != GB_HEIGHT)) ? 1 : 0;
      systemWidth = gbBorderLineSkip = GB_WIDTH;
      systemHeight = GB_HEIGHT;
      gbBorderColumnSkip = gbBorderRowSkip = 0;
      break;
   case 1:
      geometry_changed = ((systemWidth != SGB_WIDTH) || (systemHeight != SGB_HEIGHT)) ? 1 : 0;
      systemWidth = gbBorderLineSkip = SGB_WIDTH;
      systemHeight = SGB_HEIGHT;
      gbBorderColumnSkip = (SGB_WIDTH - GB_WIDTH) >> 1;
      gbBorderRowSkip = (SGB_HEIGHT - GB_HEIGHT) >> 1;
      break;
   }

   retro_get_system_av_info(&avinfo);

   if (!geometry_changed)
      environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &avinfo);
   else
      environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &avinfo);
}

#define GB_RTC_DATA_PTR  (rtcData_t *)&rtcData
#define GB_RTC_DATA_SIZE (sizeof(int) * 14 + sizeof(int64_t))

// this should probably be remapped to gbRam instead for consistency
#define GB_MBC7_SAVE_PTR  (uint8_t *)&gbMemory[GB_ADDRESS_EXTERNAL_RAM]
#define GB_MBC7_SAVE_SIZE 256

static const char *gbGetCartridgeType(void)
{
   const char *name = "Unknown";

   switch (gbRom[0x147])
   {
   case 0x00:
      name = "ROM";
      break;
   case 0x01:
      name = "ROM+MBC1";
      break;
   case 0x02:
      name = "ROM+MBC1+RAM";
      break;
   case 0x03:
      name = "ROM+MBC1+RAM+BATTERY";
      break;
   case 0x05:
      name = "ROM+MBC2";
      break;
   case 0x06:
      name = "ROM+MBC2+BATTERY";
      break;
   case 0x0b:
      name = "ROM+MMM01";
      break;
   case 0x0c:
      name = "ROM+MMM01+RAM";
      break;
   case 0x0d:
      name = "ROM+MMM01+RAM+BATTERY";
      break;
   case 0x0f:
      name = "ROM+MBC3+TIMER+BATTERY";
      break;
   case 0x10:
      name = "ROM+MBC3+TIMER+RAM+BATTERY";
      break;
   case 0x11:
      name = "ROM+MBC3";
      break;
   case 0x12:
      name = "ROM+MBC3+RAM";
      break;
   case 0x13:
      name = "ROM+MBC3+RAM+BATTERY";
      break;
   case 0x19:
      name = "ROM+MBC5";
      break;
   case 0x1a:
      name = "ROM+MBC5+RAM";
      break;
   case 0x1b:
      name = "ROM+MBC5+RAM+BATTERY";
      break;
   case 0x1c:
      name = "ROM+MBC5+RUMBLE";
      break;
   case 0x1d:
      name = "ROM+MBC5+RUMBLE+RAM";
      break;
   case 0x1e:
      name = "ROM+MBC5+RUMBLE+RAM+BATTERY";
      break;
   case 0x22:
      name = "ROM+MBC7+BATTERY";
      break;
   case 0x55:
      name = "GameGenie";
      break;
   case 0x56:
      name = "GameShark V3.0";
      break;
   case 0xfc:
      name = "ROM+POCKET CAMERA";
      break;
   case 0xfd:
      name = "ROM+BANDAI TAMA5";
      break;
   case 0xfe:
      name = "ROM+HuC-3";
      break;
   case 0xff:
      name = "ROM+HuC-1";
      break;
   }

   return name;
}

static const char *gbGetSaveRamSize(void)
{
   const char *name = "Unknown";

   switch (gbRom[0x149])
   {
   case 0:
      name = "None";
      break;
   case 1:
      name = "2K";
      break;
   case 2:
      name = "8K";
      break;
   case 3:
      name = "32K";
      break;
   case 4:
      name = "128K";
      break;
   case 5:
      name = "64K";
      break;
   }

   return name;
}

static void gb_init(void)
{
   const char *biosname[] = { "gb_bios.bin", "gbc_bios.bin" };

   log("Loading VBA-M Core (GB/GBC)...\n");

   gbGetHardwareType();

   setColorizerHack(option_colorizerHack);

   // Disable bios loading when using Colorizer hack
   if (option_colorizerHack)
      option_useBios = false;

   if (option_useBios)
   {
      snprintf(biosfile, sizeof(biosfile), "%s%c%s",
          retro_system_directory, SLASH, biosname[gbCgbMode]);
      log("Loading bios: %s\n", biosfile);
   }

   gbCPUInit(biosfile, option_useBios);

   if (gbBorderOn)
   {
      systemWidth = gbBorderLineSkip = SGB_WIDTH;
      systemHeight = SGB_HEIGHT;
      gbBorderColumnSkip = (SGB_WIDTH - GB_WIDTH) >> 1;
      gbBorderRowSkip = (SGB_HEIGHT - GB_HEIGHT) >> 1;
   }
   else
   {
      systemWidth = gbBorderLineSkip = GB_WIDTH;
      systemHeight = GB_HEIGHT;
      gbBorderColumnSkip = gbBorderRowSkip = 0;
   }

   gbSoundSetSampleRate(SAMPLERATE);
   gbSoundSetDeclicking(1);

   #define GS555(x) (x | (x << 5) | (x << 10))
   for (int i = 0; i < 24;)
   {
      systemGbPalette[i++] = GS555(0x1f);
      systemGbPalette[i++] = GS555(0x15);
      systemGbPalette[i++] = GS555(0x0c);
      systemGbPalette[i++] = 0;
   }

   gbReset(); // also resets sound;
   set_gbPalette();

   log("Rom size       : %02x (%dK)\n", gbRom[0x148], (romSize + 1023) / 1024);
   log("Cartridge      : %02x (%s)\n", gbRom[0x147], gbGetCartridgeType());
   log("Ram size       : %02x (%s)\n", gbRom[0x149], gbGetSaveRamSize());

   int i = 0;
   uint8_t crc = 25;

   for (i = 0x134; i < 0x14d; i++)
      crc += gbRom[i];
   crc = 256 - crc;
   log("CRC            : %02x (%02x)\n", crc, gbRom[0x14d]);

   uint16_t crc16 = 0;

   for (i = 0; i < gbRomSize; i++)
      crc16 += gbRom[i];

   crc16 -= gbRom[0x14e] + gbRom[0x14f];
   log("Checksum       : %04x (%04x)\n", crc16, gbRom[0x14e] * 256 + gbRom[0x14f]);

   if (gbBattery)
      log("Game supports battery save ram.\n");
   if (gbRom[0x143] == 0xc0)
      log("Game works on CGB only\n");
   else if (gbRom[0x143] == 0x80)
      log("Game supports GBC functions, GB compatible.\n");
   if (gbRom[0x146] == 0x03)
      log("Game supports SGB functions\n");
}

// GBA

typedef struct
{
   const char *romtitle;
   const char *romid;
   int saveSize; // also can override eeprom size
   int saveType; // 0auto 1eeprom 2sram 3flash 4sensor+eeprom 5none
   bool rtcEnabled;
} ini_t;

static const ini_t gbaover[] = {
#include "gba-over.inc"
};

static void gba_load_image_preferences(void)
{
   const char *savetype[] = {
      "AUTO",
      "EEPROM",
      "SRAM",
      "FLASH",
      "SENSOR + EEPROM",
      "NONE"
   };

   bool found = false;
   char buffer[12 + 1];
   unsigned i = 0, found_no = 0;
   unsigned long romCrc32 = crc32(0, rom, romSize);

   cpuSaveType = GBA_SAVE_AUTO;
   flashSize = SIZE_FLASH512;
   eepromSize = SIZE_EEPROM_512;
   rtcEnabled = false;
   mirroringEnable = false;
   hardware = HW_NONE;

   log("File CRC32      : 0x%08X\n", romCrc32);

   buffer[0] = 0;
   for (i = 0; i < 12; i++)
   {
      if (rom[0xa0 + i] == 0)
         break;
      buffer[i] = rom[0xa0 + i];
   }

   buffer[i] = 0;
   log("Game Title      : %s\n", buffer);

   buffer[0] = rom[0xac];
   buffer[1] = rom[0xad];
   buffer[2] = rom[0xae];
   buffer[3] = rom[0xaf];
   buffer[4] = 0;
   log("Game Code       : %s\n", buffer);

   for (i = 0; i < 512; i++)
   {
      if (gbaover[i].romid == NULL) break;
      if (!strcmp(gbaover[i].romid, buffer))
      {
         found = true;
         found_no = i;
         break;
      }
   }

   if (found)
   {
      log("Name            : %s\n", gbaover[found_no].romtitle);

      rtcEnabled = gbaover[found_no].rtcEnabled;
      cpuSaveType = gbaover[found_no].saveType;

      unsigned size = gbaover[found_no].saveSize;
      if (cpuSaveType == GBA_SAVE_SRAM)
         flashSize = SIZE_SRAM;
      else if (cpuSaveType == GBA_SAVE_FLASH)
         flashSize = (size == SIZE_FLASH1M) ? SIZE_FLASH1M : SIZE_FLASH512;
      else if ((cpuSaveType == GBA_SAVE_EEPROM) || (cpuSaveType == GBA_SAVE_EEPROM_SENSOR))
         eepromSize = (size == SIZE_EEPROM_8K) ? SIZE_EEPROM_8K : SIZE_EEPROM_512;
   }

   // gameID that starts with 'F' are classic/famicom games
   if (buffer[0] == 'F')
   {
      mirroringEnable = true;
      cpuSaveType = GBA_SAVE_EEPROM;
   }

   if (!cpuSaveType)
      utilGBAFindSave(romSize);

   saveType = cpuSaveType;

   if (flashSize == SIZE_FLASH512 || flashSize == SIZE_FLASH1M)
      flashSetSize(flashSize);

   if (option_forceRTCenable)
      rtcEnabled = true;

   rtcEnable(rtcEnabled);

   // Setup hardware sensors based on 1st character of game ID
   // Yoshi and Koro Koro Puzzle, tilt/accelerator
   if (buffer[0] == 'K')
      hardware = HW_TILT;
   // Wario Twister rumble + gyro
   if (buffer[0] == 'R')
      hardware = HW_RUMBLE | HW_GYRO;
   // Boktai 1 and 2, RTC + solar sensor
   if (buffer[0] == 'U')
      hardware = HW_SOLAR_SENSOR;
   // Drill Dozer, rumble
   if (buffer[0] == 'V')
      hardware = HW_RUMBLE;

   rtcEnableRumble(!rtcEnabled && (hardware & HW_RUMBLE));

   doMirroring(mirroringEnable);

   log("romSize         : %dKB\n", (romSize + 1023) / 1024);
   log("has RTC         : %s.\n", rtcEnabled ? "Yes" : "No");
   log("cpuSaveType     : %s.\n", savetype[cpuSaveType]);
   if (cpuSaveType == 3)
      log("flashSize       : %d.\n", flashSize);
   else if (cpuSaveType == 1)
      log("eepromSize      : %d.\n", eepromSize);
   log("mirroringEnable : %s.\n", mirroringEnable ? "Yes" : "No");
}

static void gba_init(void)
{
   log("Loading VBA-M Core (GBA)...\n");

   gba_load_image_preferences();
   soundSetSampleRate(SAMPLERATE);

   if (option_useBios)
   {
      snprintf(biosfile, sizeof(biosfile), "%s%c%s", retro_system_directory, SLASH, "gba_bios.bin");
      log("Loading bios: %s\n", biosfile);
   }
   CPUInit(biosfile, option_useBios);

   systemWidth = GBA_WIDTH;
   systemHeight = GBA_HEIGHT;

   CPUReset();
}

static void update_variables(bool startup)
{
   struct retro_variable var = { 0 };
   char key[256] = { 0 };
   int disabled_layers = 0;
   int sound_enabled = 0x30F;
   bool sound_changed = false;

   var.key = key;
   strcpy(key, "vbam_layer_x");
   for (int i = 0; i < 8; i++)
   {
      key[strlen("vbam_layer_")] = '1' + i;
      var.value = NULL;
      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value && var.value[0] == 'd')
         disabled_layers |= 0x100 << i;
   }

   layerSettings = 0xFF00 ^ disabled_layers;
   layerEnable = lcd.dispcnt & layerSettings;
   CPUUpdateRenderBuffers(false);

   strcpy(key, "vbam_sound_x");
   for (unsigned i = 0; i < 6; i++)
   {
      key[strlen("vbam_sound_")] = '1' + i;
      var.value = NULL;
      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value && var.value[0] == 'd')
      {
         int which = (i < 4) ? (1 << i) : (0x100 << (i - 4));
         sound_enabled &= ~(which);
      }
   }

   if (soundGetEnable() != sound_enabled)
      soundSetEnable(sound_enabled & 0x30F);

   var.key = "vbam_soundinterpolation";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      bool newval = (!strcmp(var.value, "enabled"));
      if (option_sndInterpolation != newval)
      {
         option_sndInterpolation = newval;
         sound_changed = true;
      }
   }

   var.key = "vbam_soundfiltering";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      double newval = atof(var.value) * 0.1f;
      if (option_sndFiltering != newval)
      {
         option_sndFiltering = newval;
         sound_changed = true;
      }
   }

   if (sound_changed)
   {
      soundInterpolation = option_sndInterpolation;
      soundFiltering = option_sndFiltering;
   }

   var.key = "vbam_usebios";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      option_useBios = (!strcmp(var.value, "enabled")) ? true : false;
   }

   var.key = "vbam_forceRTCenable";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      option_forceRTCenable = (!strcmp(var.value, "enabled")) ? true : false;
   }

   var.key = "vbam_solarsensor";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int newval = atoi(var.value);
      update_input_solar_sensor(newval);
   }

   var.key = "vbam_showborders";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "auto") == 0)
      {
         gbBorderAutomatic = 1;
      }
      else if (!strcmp(var.value, "enabled"))
      {
         gbBorderAutomatic = 0;
         gbBorderOn = 1;
      }
      else
      { // disabled
         gbBorderOn = 0;
         gbBorderAutomatic = 0;
      }

      if (core && (core->type == IMAGE_GB) && !startup)
         SetGBBorder(gbBorderOn);
   }

   var.key = "vbam_gbHardware";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value && startup)
   {
      if (strcmp(var.value, "auto") == 0)
         gbEmulatorType = 0;
      else if (strcmp(var.value, "gbc") == 0)
         gbEmulatorType = 1;
      else if (strcmp(var.value, "sgb") == 0)
         gbEmulatorType = 2;
      else if (strcmp(var.value, "gb") == 0)
         gbEmulatorType = 3;
      else if (strcmp(var.value, "gba") == 0)
         gbEmulatorType = 4;
      else if (strcmp(var.value, "sgb2") == 0)
         gbEmulatorType = 5;
   }

   var.key = "vbam_allowcolorizerhack";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      option_colorizerHack = (!strcmp(var.value, "enabled")) ? true : false;
   }

   var.key = "vbam_turboenable";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      option_turboEnable = (!strcmp(var.value, "enabled")) ? true : false;
   }

   var.key = "vbam_turbodelay";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      option_turboDelay = atoi(var.value);
   }

   var.key = "vbam_astick_deadzone";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      option_analogDeadzone = (int)(atof(var.value) * 0.01 * 0x8000);
   }

   var.key = "vbam_tilt_sensitivity";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      option_tiltSensitivity = atoi(var.value);
   }

   var.key = "vbam_gyro_sensitivity";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      option_gyroSensitivity = atoi(var.value);
   }

   var.key = "vbam_swap_astick";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      option_swapAnalogSticks = (!strcmp(var.value, "enabled")) ? true : false;
   }

   var.key = "vbam_palettes";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      unsigned lastpal = option_gbPalette;

      if (!strcmp(var.value, "black and white"))
         option_gbPalette = 0;
      else if (!strcmp(var.value, "blue sea"))
         option_gbPalette = 1;
      else if (!strcmp(var.value, "dark knight"))
         option_gbPalette = 2;
      else if (!strcmp(var.value, "green forest"))
         option_gbPalette = 3;
      else if (!strcmp(var.value, "hot desert"))
         option_gbPalette = 4;
      else if (!strcmp(var.value, "pink dreams"))
         option_gbPalette = 5;
      else if (!strcmp(var.value, "wierd colors"))
         option_gbPalette = 6;
      else if (!strcmp(var.value, "original gameboy"))
         option_gbPalette = 7;
      else if (!strcmp(var.value, "gba sp"))
         option_gbPalette = 8;

      if (lastpal != option_gbPalette && !startup)
         set_gbPalette();
   }

   var.key = "vbam_gbcoloroption";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      gbColorOption = (!strcmp(var.value, "enabled")) ? 1 : 0;
   }

   var.key = "vbam_show_advanced_options";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      bool newval = (!strcmp(var.value, "enabled")) ? true : false;
      if ((option_showAdvancedOptions != newval) || startup)
      {
         option_showAdvancedOptions = newval;
         struct retro_core_option_display option_display;
         unsigned i;
         char options[][13] = {
            "vbam_sound_1",
            "vbam_sound_2",
            "vbam_sound_3",
            "vbam_sound_4",
            "vbam_sound_5",
            "vbam_sound_6",
            "vbam_layer_1",
            "vbam_layer_2",
            "vbam_layer_3",
            "vbam_layer_4",
            "vbam_layer_5",
            "vbam_layer_6",
            "vbam_layer_7",
            "vbam_layer_8"
         };
         option_display.visible = option_showAdvancedOptions;
         for (i = 0; i < (sizeof(options) / sizeof(options[0])); i++)
         {
            option_display.key = options[i];
            environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
         }
      }
   }

   // Hide some core options depending on rom image type
   //if (startup)
   {
      unsigned i;
      struct retro_core_option_display option_display;
      char gb_options[5][25] = {
         "vbam_palettes",
         "vbam_gbHardware",
         "vbam_allowcolorizerhack",
         "vbam_showborders",
         "vbam_gbcoloroption"
      };
      char gba_options[3][22] = {
         "vbam_solarsensor",
         "vbam_gyro_sensitivity",
         "vbam_forceRTCenable"
      };

      // Show or hide GB/GBC only options
      option_display.visible = (core && core->type == IMAGE_GB) ? 1 : 0;
      for (i = 0; i < 5; i++)
      {
         option_display.key = gb_options[i];
         environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
      }

      // Show or hide GBA only options
      option_display.visible = (core && core->type == IMAGE_GBA) ? 1 : 0;
      for (i = 0; i < 3; i++)
      {
         option_display.key = gba_options[i];
         environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
      }
   }
}

// LIBRETRO API

static bool firstrun = true;
static unsigned has_frame;
static int16_t soundbuf[SOUND_BUFFER_SIZE];
static unsigned serialize_size = 0;

RETRO_API void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;
   libretro_set_core_options(environ_cb);
}

RETRO_API void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

RETRO_API void retro_set_audio_sample(retro_audio_sample_t cb)
{
}

RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

RETRO_API void retro_set_input_poll(retro_input_poll_t cb)
{
   poll_cb = cb;
}

RETRO_API void retro_set_input_state(retro_input_state_t cb)
{
   input_cb = cb;
}

RETRO_API void retro_init(void)
{
   struct retro_log_callback log;

   environ_cb(RETRO_ENVIRONMENT_GET_CAN_DUPE, &can_dupe);
   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
   else
      log_cb = NULL;

   const char *dir = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir)
      snprintf(retro_system_directory, sizeof(retro_system_directory), "%s", dir);

#ifdef FRONTEND_SUPPORTS_RGB565
   systemColorDepth = 16;
   systemRedShift = 11;
   systemGreenShift = 6;
   systemBlueShift = 0;
   enum retro_pixel_format rgb565 = RETRO_PIXEL_FORMAT_RGB565;
   if (environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &rgb565) && log_cb)
      log_cb(RETRO_LOG_INFO, "Frontend supports RGB565 - will use that instead of XRGB1555.\n");
#else
   systemColorDepth = 32;
   systemRedShift = 19;
   systemGreenShift = 11;
   systemBlueShift = 3;
   enum retro_pixel_format rgb8888 = RETRO_PIXEL_FORMAT_XRGB8888;
   if (environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &rgb8888) && log_cb)
      log_cb(RETRO_LOG_INFO, "Frontend supports XRGB8888 - will use that instead of XRGB1555.\n");
#endif

   bool yes = true;
   environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS, &yes);

   if (environ_cb(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL))
      libretro_supports_bitmasks = true;
}

RETRO_API void retro_deinit(void)
{
   emulating = 0;
   core->emuCleanUp();
   soundShutdown();
   libretro_supports_bitmasks = false;
   can_dupe = false;
}

RETRO_API unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

RETRO_API void retro_get_system_info(struct retro_system_info *info)
{
   info->need_fullpath = false;
   info->valid_extensions = "dmg|gb|gbc|cgb|sgb|gba";
#ifndef VBAM_VERSION
#define VBAM_VERSION "2.1.3"
#endif
#ifdef GIT_COMMIT
   info->library_version = VBAM_VERSION " " GIT_COMMIT;
#else
   info->library_version = VBAM_VERSION;
#endif
   info->library_name = "VBA-M";
   info->block_extract = false;
}

RETRO_API void retro_get_system_av_info(struct retro_system_av_info *info)
{
   double aspect      = (3.0 / 2.0);
   unsigned maxWidth  = GBA_WIDTH;
   unsigned maxHeight = GBA_HEIGHT;

   if (core->type == IMAGE_GB)
   {
      aspect    = !gbBorderOn ? (10.0 / 9.0) : (8.0 / 7.0);
      maxWidth  = !gbBorderOn ? GB_WIDTH : SGB_WIDTH;
      maxHeight = !gbBorderOn ? GB_HEIGHT : SGB_HEIGHT;
   }

   info->geometry.base_width   = systemWidth;
   info->geometry.base_height  = systemHeight;
   info->geometry.max_width    = maxWidth;
   info->geometry.max_height   = maxHeight;
   info->geometry.aspect_ratio = aspect;
   info->timing.fps            = FRAMERATE;
   info->timing.sample_rate    = SAMPLERATE;
}

RETRO_API void retro_set_controller_port_device(unsigned port, unsigned device)
{
   if (port > 3) return;

   input_type[port] = device;
   log_cb(RETRO_LOG_INFO, "Controller %d device: %d\n", port + 1, device);
}

RETRO_API void retro_reset(void)
{
   core->emuReset();
   set_gbPalette();
}

RETRO_API void retro_run(void)
{
   bool updated = false;   

   if (firstrun)
   {
      firstrun = false;
      /* Check if GB game has RTC data. Has to be check here since this is where the data will be
       * available when using libretro api. */
      if ((core->type == IMAGE_GB) && gbRTCPresent)
      {
         /* Check if any RTC has been loaded, zero value means nothing has been loaded. */
         if (!rtcData.mapperSeconds && !rtcData.mapperLSeconds && rtcData.mapperLastTime == (time_t)-1)
         {
            /* Initialize RTC */
            gbInitRTC();
         }
      }
   }

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      update_variables(false);

   poll_cb();

   update_input(libretro_supports_bitmasks, input_cb);

   has_frame = 0;
   core->emuMain(core->emuCount, input_buf);
   video_cb(has_frame ? pix : NULL, systemWidth, systemHeight, systemWidth * sizeof(pixFormat));

   int soundlen = core->emuFlushAudio(soundbuf);
   audio_batch_cb((const int16_t*)soundbuf, soundlen >> 1);
}

RETRO_API size_t retro_serialize_size(void)
{
   return serialize_size;
}

RETRO_API bool retro_serialize(void *data, size_t size)
{
   if (size == serialize_size)
      return core->emuWriteState((uint8_t *)data, size);
   return false;
}

RETRO_API bool retro_unserialize(const void *data, size_t size)
{
   if (size == serialize_size)
      return core->emuReadState((uint8_t *)data, size);
   return false;
}

RETRO_API void retro_cheat_reset(void)
{
   cheatsEnabled = 1;
   if (core->type == IMAGE_GBA)
      cheatsDeleteAll(false);
   else if (core->type == IMAGE_GB)
      gbCheatRemoveAll();
}

#define ISHEXDEC \
   ((code[cursor] >= '0') && (code[cursor] <= '9')) || \
   ((code[cursor] >= 'a') && (code[cursor] <= 'f')) || \
   ((code[cursor] >= 'A') && (code[cursor] <= 'F')) || \
   (code[cursor] == '-')

RETRO_API void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   // 2018-11-30 - retrowertz
   // added support GB/GBC multiline 6/9 digit Game Genie codes and Game Shark

   char name[128] = { 0 };
   unsigned cursor = 0;
   char *codeLine = NULL;
   int codeLineSize = strlen(code) + 5;
   int codePos = 0;
   int i = 0;

   codeLine = (char *)calloc(codeLineSize, sizeof(char));
   sprintf(name, "cheat_%d", index);
   for (cursor = 0;; cursor++)
   {
      if (ISHEXDEC)
      {
         codeLine[codePos++] = toupper(code[cursor]);
      }
      else
      {
         switch (core->type)
         {
         case IMAGE_GB:
            if (codePos >= 7)
            {
               if (codePos == 7 || codePos == 11)
               {
                  codeLine[codePos] = '\0';
                  if (gbAddGgCheat(codeLine, name))
                     log("Cheat code added: '%s'\n", codeLine);
               }
               else if (codePos == 8)
               {
                  codeLine[codePos] = '\0';
                  if (gbAddGsCheat(codeLine, name))
                     log("Cheat code added: '%s'\n", codeLine);
               }
               else
               {
                  codeLine[codePos] = '\0';
                  log("Invalid cheat code '%s'\n", codeLine);
               }
               codePos = 0;
               memset(codeLine, 0, codeLineSize);
            }
            break;
         case IMAGE_GBA:
            if (codePos >= 12)
            {
               if (codePos == 12)
               {
                  for (i = 0; i < 4; i++)
                     codeLine[codePos - i] = codeLine[(codePos - i) - 1];
                  codeLine[8] = ' ';
                  codeLine[13] = '\0';
                  cheatsAddCBACode(codeLine, name);
                  log("Cheat code added: '%s'\n", codeLine);
               }
               else if (codePos == 16)
               {
                  codeLine[16] = '\0';
                  cheatsAddGSACode(codeLine, name, true);
                  log("Cheat code added: '%s'\n", codeLine);
               }
               else
               {
                  codeLine[codePos] = '\0';
                  log("Invalid cheat code '%s'\n", codeLine);
               }
               codePos = 0;
               memset(codeLine, 0, codeLineSize);
            }
            break;
         default: break;
         }
         if (!code[cursor])
            break;
      }
   }
   free(codeLine);
}

RETRO_API bool retro_load_game(const struct retro_game_info *game)
{
   if (!game)
      return false;
   
   IMAGE_TYPE type = utilFindType((const char *)game->path);

   if (type == IMAGE_UNKNOWN)
   {
      log("Unsupported image %s!\n", game->path);
      return false;
   }

   update_variables(true);
   utilUpdateSystemColorMaps(false);
   soundInit();

   struct retro_memory_descriptor desc[18];
   struct retro_memory_map retromap;

   memset(desc, 0, sizeof(desc));

   if (type == IMAGE_GBA)
   {
      romSize = CPULoadRomData((const char *)game->data, game->size);

      if (!romSize)
         return false;

      core = &GBASystem;

      gba_init();

      unsigned i = 0;

      desc[i].start     = 0x03000000;
      desc[i].len       = 0x8000;
      desc[i].ptr       = internalRAM;
      desc[i].addrspace = "IWRAM";
      i++;

      desc[i].start     = 0x02000000;
      desc[i].len       = 0x40000;
      desc[i].ptr       = workRAM;
      desc[i].addrspace = "EWRAM";
      i++;

      if (cpuSramEnabled || cpuFlashEnabled)
      {
         desc[i].start     = 0x0E000000;
         desc[i].len       = flashSize;
         desc[i].ptr       = flashSaveMemory;
         desc[i].addrspace = cpuSramEnabled ? "SRAM" : "FLASH";
         i++;
      }

      desc[i].start     = 0x00000000;
      desc[i].len       = 0x4000;
      desc[i].ptr       = bios;
      desc[i].flags     = RETRO_MEMDESC_CONST;
      desc[i].addrspace = "BIOS";
      i++;

      desc[i].start     = 0x04000000;
      desc[i].len       = 0x400;
      desc[i].ptr       = ioMem;
      desc[i].addrspace = "IOREG";
      i++;

      desc[i].start     = 0x05000000;
      desc[i].len       = 0x400;
      desc[i].ptr       = paletteRAM;
      desc[i].addrspace = "PRAM";
      i++;

      desc[i].start     = 0x06000000;
      desc[i].select    = 0xFF000000;
      desc[i].len       = 0x18000;
      desc[i].ptr       = vram;
      desc[i].addrspace = "VRAM";
      i++;

      desc[i].start     = 0x07000000;
      desc[i].len       = 0x400;
      desc[i].ptr       = oam;
      desc[i].addrspace = "OAM";
      i++;

      desc[i].start     = 0x08000000;
      desc[i].len       = romSize;
      desc[i].ptr       = rom;
      desc[i].flags     = RETRO_MEMDESC_CONST;
      desc[i].addrspace = "ROMWAIT0";
      i++;

      desc[i].start     = 0x0A000000;
      desc[i].len       = romSize;
      desc[i].ptr       = rom;
      desc[i].flags     = RETRO_MEMDESC_CONST;
      desc[i].addrspace = "ROMWAIT1";
      i++;

      desc[i].start     = 0x0C000000;
      desc[i].len       = romSize;
      desc[i].ptr       = rom;
      desc[i].flags     = RETRO_MEMDESC_CONST;
      desc[i].addrspace = "ROMWAIT2";
      i++;

      retromap.descriptors = desc;
      retromap.num_descriptors = i;
      environ_cb(RETRO_ENVIRONMENT_SET_MEMORY_MAPS, &retromap);
   }

   else if (type == IMAGE_GB)
   {
      if (!gbLoadRomData((const char *)game->data, game->size))
         return false;

      romSize = game->size;

      core = &GBSystem;

      gb_init();

      hardware = HW_NONE;
      if (gbDataMBC5.isRumbleCartridge) hardware |= HW_RUMBLE;
      if (gbRomType == 0x22) hardware |= HW_TILT;

      // the GB's memory is divided into 16 parts with 4096 bytes each == 65536
      // $FFFF        Interrupt Enable Flag
      // $FF80-$FFFE  Zero Page - 127 bytes
      // $FF00-$FF7F  Hardware I/O Registers
      // $FEA0-$FEFF  Unusable Memory
      // $FE00-$FE9F  OAM - Object Attribute Memory
      // $E000-$FDFF  Echo RAM - Reserved, Do Not Use
      // $D000-$DFFF  Internal RAM - Bank 1-7 (switchable - CGB only)
      // $C000-$CFFF  Internal RAM - Bank 0 (fixed)
      // $A000-$BFFF  Cartridge RAM (If Available)
      // $9C00-$9FFF  BG Map Data 2
      // $9800-$9BFF  BG Map Data 1
      // $8000-$97FF  Character RAM
      // $4000-$7FFF  Cartridge ROM - Switchable Banks 1-xx
      // $0150-$3FFF  Cartridge ROM - Bank 0 (fixed)
      // $0100-$014F  Cartridge Header Area
      // $0000-$00FF  Restart and Interrupt Vectors
      // http://gameboy.mongenel.com/dmg/asmmemmap.html

      unsigned i = 0;

      desc[i].ptr       = gbCgbMode ? gbWram : gbMemory;
      desc[i].offset    = gbCgbMode ? 0 : 0xC000;
      desc[i].len       = GB_SIZE_WRAM_BANK0 + GB_SIZE_WRAM_BANK1;
      desc[i].start     = 0xC000;
      desc[i].addrspace = "WRAM";
      i++;

      bool useRam       = gbRamSize && gbRam;
      desc[i].ptr       = useRam ? gbRam : gbMemory;
      desc[i].offset    = useRam ? 0 : 0xA000;
      desc[i].len       = useRam ? gbRamSize : GB_SIZE_EXTERNAL_RAM;
      desc[i].start     = 0xA000;
      desc[i].addrspace = "ERAM";
      i++;

      desc[i].start     = 0x0000;
      desc[i].len       = std::min<int>(gbRomSize, 0x8000);
      desc[i].ptr       = gbRom;
      desc[i].addrspace = "ROM";
      i++;

      desc[i].ptr       = gbCgbMode ? gbVram : gbMemory;
      desc[i].offset    = gbCgbMode ? 0 : 0x8000;
      desc[i].len       = GB_SIZE_VRAM;
      desc[i].start     = 0x8000;
      desc[i].addrspace = "VRAM";
      i++;

      desc[i].ptr       = gbMemory;
      desc[i].offset    = 0xF000;
      desc[i].len       = 0x1000;
      desc[i].start     = 0xF000;
      desc[i].addrspace = "OTHERS";
      i++;

      if (gbWram && gbCgbMode)
      { 
         // banks 2-7 of GBC work ram banks at $10000
         desc[i].ptr    = gbWram;
         desc[i].offset = 0x2000;
         desc[i].start  = 0x10000;
         desc[i].select = 0xFFFFA000;
         desc[i].len    = 0x6000;
         i++;
      }

      retromap.descriptors = desc;
      retromap.num_descriptors = i;
      environ_cb(RETRO_ENVIRONMENT_SET_MEMORY_MAPS, &retromap);
   }

   if (!core)
      return false;

   update_input_descriptors(core, environ_cb); // Initialize input descriptors and info
   update_variables(false);
   uint8_t *state_buf = (uint8_t *)malloc(2000000);
   serialize_size = core->emuWriteState(state_buf, 2000000);
   free(state_buf);

   emulating = 1;

   return emulating;
}

RETRO_API bool retro_load_game_special(unsigned, const struct retro_game_info *, size_t)
{
   return false;
}

void retro_unload_game(void)
{
}

RETRO_API unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

RETRO_API void *retro_get_memory_data(unsigned id)
{
   void *data = NULL;

   switch (core->type)
   {
   case IMAGE_GBA:
      switch (id)
      {
      case RETRO_MEMORY_SAVE_RAM:
         if ((saveType == GBA_SAVE_EEPROM) | (saveType == GBA_SAVE_EEPROM_SENSOR))
            data = eepromData;
         else if ((saveType == GBA_SAVE_SRAM) | (saveType == GBA_SAVE_FLASH))
            data = flashSaveMemory;
         break;
      }
      break;

   case IMAGE_GB:
      switch (id)
      {
      case RETRO_MEMORY_SAVE_RAM:
         if (gbBattery)
         {
            if (mapperType == MBC7)
               data = GB_MBC7_SAVE_PTR;
            else
               data = gbRam;
         }
         break;
      case RETRO_MEMORY_RTC:
         if (gbRTCPresent)
            data = GB_RTC_DATA_PTR;
         break;
      }
      break;

   default: break;
   }
   return data;
}

RETRO_API size_t retro_get_memory_size(unsigned id)
{
   size_t size = 0;

   switch (core->type)
   {
   case IMAGE_GBA:
      switch (id)
      {
      case RETRO_MEMORY_SAVE_RAM:
         if ((saveType == GBA_SAVE_EEPROM) | (saveType == GBA_SAVE_EEPROM_SENSOR))
            size = eepromSize;
         else if (saveType == GBA_SAVE_FLASH)
            size = flashSize;
         else if (saveType == GBA_SAVE_SRAM)
            size = SIZE_SRAM;
         break;
      }
      break;

   case IMAGE_GB:
      switch (id)
      {
      case RETRO_MEMORY_SAVE_RAM:
         if (gbBattery)
         {
            if (mapperType == MBC7)
               size = GB_MBC7_SAVE_SIZE;
            else
               size = gbRamSize;
         }
         break;
      case RETRO_MEMORY_RTC:
         if (gbRTCPresent)
            size = GB_RTC_DATA_SIZE;
         break;
      }
      break;

   default: break;
   }
   return size;
}

// system callbacks

void systemOnWriteDataToSoundBuffer(const uint16_t *, int)
{
}

void systemOnSoundShutdown(void)
{
}

bool systemCanChangeSoundQuality(void)
{
   return true;
}

void systemDrawScreen(uint16_t *pix)
{
   has_frame++;
}

void systemFrame(void)
{
}

void systemGbBorderOn(void)
{
   SetGBBorder(1);
}

void systemMessage(const char *fmt, ...)
{
   char buffer[256];
   va_list ap;
   va_start(ap, fmt);
   vsprintf(buffer, fmt, ap);
   if (log_cb)
      log_cb(RETRO_LOG_INFO, "%s\n", buffer);
   va_end(ap);
}

void systemMessage(int, const char *fmt, ...)
{
   char buffer[256];
   va_list ap;
   va_start(ap, fmt);
   vsprintf(buffer, fmt, ap);
   if (log_cb)
      log_cb(RETRO_LOG_INFO, "%s\n", buffer);
   va_end(ap);
}

uint32_t systemReadJoypad(int which)
{
   return 0;
}

bool systemReadJoypads(void)
{
   return true;
}

int systemGetSensorX()
{
   return get_sensor_tilt_x();
}

int systemGetSensorY()
{
   return get_sensor_tilt_y();
}

int systemGetSensorZ()
{
   return get_sensor_gyro() / 10;
}

uint8_t systemGetSensorDarkness(void)
{
   return get_sensor_solar();
}

void systemUpdateMotionSensor(void)
{
}

void systemCartridgeRumble(bool e)
{
   if (!(hardware & HW_RUMBLE))
      return;

   update_input_rumble_state(e);
}

bool systemPauseOnFrame(void)
{
   return false;
}

void systemGbPrint(uint8_t *, int, int, int, int)
{
}

void systemScreenCapture(int)
{
}

void systemScreenMessage(const char *msg)
{
   if (log_cb)
      log_cb(RETRO_LOG_INFO, "%s\n", msg);
}

void systemSetTitle(const char *)
{
}

void systemShowSpeed(int)
{
}

void system10Frames(int)
{
}

uint32_t systemGetClock(void)
{
   return 0;
}

SoundDriver *systemSoundInit(void)
{
   soundShutdown();
   return new SoundRetro();
}

void log(const char *defaultMsg, ...)
{
   va_list valist;
   char buf[2048];
   va_start(valist, defaultMsg);
   vsnprintf(buf, 2048, defaultMsg, valist);
   va_end(valist);

   if (log_cb)
      log_cb(RETRO_LOG_INFO, "%s", buf);
}
