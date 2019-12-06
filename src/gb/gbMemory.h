#ifndef GBMEMORY_H
#define GBMEMORY_H

#include "../common/Types.h"
#include <time.h>

#define GB_ADDRESS_ROM_BANK0    0x0000
#define GB_ADDRESS_ROM_BANK1    0x4000
#define GB_ADDRESS_VRAM         0x8000
#define GB_ADDRESS_EXTERNAL_RAM 0xA000
#define GB_ADDRESS_WRAM_BANK0   0xC000
#define GB_ADDRESS_WRAM_BANK1   0xD000
#define GB_ADDRESS_MIRROR       0xE000
#define GB_ADDRESS_OAM          0xFE00
#define GB_ADDRESS_UNUSUABLE    0xFEA0
#define GB_ADDRESS_IO           0xFF00
#define GB_ADDRESS_HRAM         0xFF80
#define GB_ADDRESS_IE           0xFFFF

#define GB_SIZE_ROM_BANK        0x4000
#define GB_SIZE_VRAM            0x2000
#define GB_SIZE_EXTERNAL_RAM    0x2000
#define GB_SIZE_WRAM_BANK0      0x1000
#define GB_SIZE_WRAM_BANK1      0x1000
#define GB_SIZE_MIRROR          0x2E00
#define GB_SIZE_OAM             0x00A0
#define GB_SIZE_UNREADABLE      0x0060
#define GB_SIZE_IO              0x0080
#define GB_SIZE_HRAM            0x007F
#define GB_SIZE_IE              0x0001

enum gb_memory_base_address
{
   GB_MEM_CART_BANK0 = 0x0,
   GB_MEM_CART_BANK1 = 0x4,
   GB_MEM_VRAM = 0x8,
   GB_MEM_EXTERNAL_RAM = 0xA,
   GB_MEM_WRAM_BANK0 = 0xC,
   GB_MEM_WRAM_BANK1 = 0xD,
   GB_MEM_BASE_E = 0xE,
   GB_MEM_BASE_F = 0xF
};

enum gb_registers
{
   //Joypad
   REG_P1 = 0xFF00,

   //Serial Input-Output
   REG_SB = 0xFF01,
   REG_SC = 0xFF02,

   //Timer
   REG_DIV = 0xFF04,
   REG_TIMA = 0xFF05,
   REG_TMA = 0xFF06,
   REG_TAC = 0xFF07,

   //Interrupt Flags
   IF_FLAG = 0xFF0F,
   IE_FLAG = 0xFFFF,

   //Sound registers
   REG_NR10 = 0xFF10,
   REG_NR11 = 0xFF11,
   REG_NR12 = 0xFF12,
   REG_NR13 = 0xFF13,
   REG_NR14 = 0xFF14,

   REG_NR21 = 0xFF16,
   REG_NR22 = 0xFF17,
   REG_NR23 = 0xFF18,
   REG_NR24 = 0xFF19,

   REG_NR30 = 0xFF1A,
   REG_NR31 = 0xFF1B,
   REG_NR32 = 0xFF1C,
   REG_NR33 = 0xFF1D,
   REG_NR34 = 0xFF1E,

   REG_NR41 = 0xFF20,
   REG_NR42 = 0xFF21,
   REG_NR43 = 0xFF22,
   REG_NR44 = 0xFF23,

   REG_NR50 = 0xFF24,
   REG_NR51 = 0xFF25,
   REG_NR52 = 0xFF26,

   REG_WAVE_0 = 0xFF30,
   REG_WAVE_1 = 0xFF31,
   REG_WAVE_2 = 0xFF32,
   REG_WAVE_3 = 0xFF33,
   REG_WAVE_4 = 0xFF34,
   REG_WAVE_5 = 0xFF35,
   REG_WAVE_6 = 0xFF36,
   REG_WAVE_7 = 0xFF37,
   REG_WAVE_8 = 0xFF38,
   REG_WAVE_9 = 0xFF39,
   REG_WAVE_A = 0xFF3A,
   REG_WAVE_B = 0xFF3B,
   REG_WAVE_C = 0xFF3C,
   REG_WAVE_D = 0xFF3D,
   REG_WAVE_E = 0xFF3E,
   REG_WAVE_F = 0xFF3F,

   //Display registers
   REG_LCDC = 0xFF40,
   REG_STAT = 0xFF41,
   REG_SCY = 0xFF42,
   REG_SCX = 0xFF43,
   REG_LY = 0xFF44,
   REG_LYC = 0xFF45,
   REG_DMA = 0xFF46,
   REG_BGP = 0xFF47,
   REG_OBP0 = 0xFF48,
   REG_OBP1 = 0xFF49,
   REG_WY = 0xFF4A,
   REG_WX = 0xFF4B,

   //Double Speed Control
   REG_KEY1 = 0xFF4D,

   //Video RAM Bank
   REG_VBK = 0xFF4F,

   //HDMA
   REG_HDMA1 = 0xFF51,
   REG_HDMA2 = 0xFF52,
   REG_HDMA3 = 0xFF53,
   REG_HDMA4 = 0xFF54,
   REG_HDMA5 = 0xFF55,

   //GBC Infrared Communications Port
   REG_RP = 0xFF56,

   //GBC palettes
   REG_BCPS = 0xFF68,
   REG_BCPD = 0xFF69,
   REG_OCPS = 0xFF6A,
   REG_OCPD = 0xFF6B,

   // WRAM Bank
   REG_SVBK = 0xFF70,

   REG_UNK6C = 0xFF6C,
   REG_UNK75 = 0xFF75
};

struct rtcData_t
{
   int mapperSeconds;
   int mapperMinutes;
   int mapperHours;
   int mapperDays;
   int mapperMonths;
   int mapperYears;
   int mapperControl;
   int mapperLSeconds;
   int mapperLMinutes;
   int mapperLHours;
   int mapperLDays;
   int mapperLMonths;
   int mapperLYears;
   int mapperLControl;
   union {
      time_t mapperLastTime;
      int64_t _time_pad; /* so that 32bit and 64bit saves are compatible */
   };
};

struct mapperMBC1
{
   int mapperRAMEnable;
   int mapperROMBank;
   int mapperRAMBank;
   int mapperMemoryModel;
   int mapperROMHighAddress;
   int mapperRAMAddress;
   int mapperRomBank0Remapping;
};

struct mapperMBC2
{
   int mapperRAMEnable;
   int mapperROMBank;
};

struct mapperMBC3
{
   int mapperRAMEnable;
   int mapperROMBank;
   int mapperRAMBank;
   int mapperRAMAddress;
   int mapperClockLatch;
   int mapperClockRegister;
};

struct mapperMBC5
{
   int mapperRAMEnable;
   int mapperROMBank;
   int mapperRAMBank;
   int mapperROMHighAddress;
   int mapperRAMAddress;
   int isRumbleCartridge;
};

struct mapperMBC7
{
   int mapperRAMEnable;
   int mapperROMBank;
   int mapperRAMBank;
   int mapperRAMAddress;
   int cs;
   int sk;
   int state;
   int buffer;
   int idle;
   int count;
   int code;
   int address;
   int writeEnable;
   int value;
};

struct mapperHuC1
{
   int mapperRAMEnable;
   int mapperROMBank;
   int mapperRAMBank;
   int mapperMemoryModel;
   int mapperROMHighAddress;
   int mapperRAMAddress;
};

struct mapperHuC3
{
   int mapperRAMEnable;
   int mapperROMBank;
   int mapperRAMBank;
   int mapperRAMAddress;
   int mapperAddress;
   int mapperRAMFlag;
   int mapperRAMValue;
   int mapperRegister1;
   int mapperRegister2;
   int mapperRegister3;
   int mapperRegister4;
   int mapperRegister5;
   int mapperRegister6;
   int mapperRegister7;
   int mapperRegister8;
};

struct mapperTAMA5
{
   int mapperRAMEnable;
   int mapperROMBank;
   int mapperRAMBank;
   int mapperRAMAddress;
   int mapperRamByteSelect;
   int mapperCommandNumber;
   int mapperLastCommandNumber;
   int mapperCommands[0x10];
   int mapperRegister;
   int mapperClockLatch;
   int mapperClockRegister;
};

struct mapperMMM01
{
   int mapperRAMEnable;
   int mapperROMBank;
   int mapperRAMBank;
   int mapperMemoryModel;
   int mapperROMHighAddress;
   int mapperRAMAddress;
   int mapperRomBank0Remapping;
};

struct mapperGS3
{
   int mapperROMBank;
};

extern mapperMBC1 gbDataMBC1;
extern mapperMBC2 gbDataMBC2;
extern mapperMBC3 gbDataMBC3;
extern mapperMBC5 gbDataMBC5;
extern mapperHuC1 gbDataHuC1;
extern mapperHuC3 gbDataHuC3;
extern mapperTAMA5 gbDataTAMA5;
extern mapperMMM01 gbDataMMM01;
extern mapperGS3 gbDataGS3;
extern rtcData_t rtcData;


void mapperMBC1ROM(uint16_t, uint8_t);
void mapperMBC1RAM(uint16_t, uint8_t);
uint8_t mapperMBC1ReadRAM(uint16_t);
void mapperMBC2ROM(uint16_t, uint8_t);
void mapperMBC2RAM(uint16_t, uint8_t);
void mapperMBC3ROM(uint16_t, uint8_t);
void mapperMBC3RAM(uint16_t, uint8_t);
uint8_t mapperMBC3ReadRAM(uint16_t);
void mapperMBC5ROM(uint16_t, uint8_t);
void mapperMBC5RAM(uint16_t, uint8_t);
uint8_t mapperMBC5ReadRAM(uint16_t);
void mapperMBC7ROM(uint16_t, uint8_t);
void mapperMBC7RAM(uint16_t, uint8_t);
uint8_t mapperMBC7ReadRAM(uint16_t);
void mapperHuC1ROM(uint16_t, uint8_t);
void mapperHuC1RAM(uint16_t, uint8_t);
void mapperHuC3ROM(uint16_t, uint8_t);
void mapperHuC3RAM(uint16_t, uint8_t);
uint8_t mapperHuC3ReadRAM(uint16_t);
void mapperTAMA5RAM(uint16_t, uint8_t);
uint8_t mapperTAMA5ReadRAM(uint16_t);
void memoryUpdateTAMA5Clock();
void mapperMMM01ROM(uint16_t, uint8_t);
void mapperMMM01RAM(uint16_t, uint8_t);
void mapperGGROM(uint16_t, uint8_t);
void mapperGS3ROM(uint16_t, uint8_t);
// extern void (*mapper)(uint16_t,uint8_t);
// extern void (*mapperRAM)(uint16_t,uint8_t);
// extern uint8_t (*mapperReadRAM)(uint16_t);

extern void memoryUpdateMapMBC1();
extern void memoryUpdateMapMBC2();
extern void memoryUpdateMapMBC3();
extern void memoryUpdateMapMBC5();
extern void memoryUpdateMapMBC7();
extern void memoryUpdateMapHuC1();
extern void memoryUpdateMapHuC3();
extern void memoryUpdateMapTAMA5();
extern void memoryUpdateMapMMM01();
extern void memoryUpdateMapGS3();

#define MBC3_RTC_DATA_SIZE sizeof(int) * 10 + sizeof(uint64_t)

#define TAMA5_RTC_DATA_SIZE sizeof(int) * 14 + sizeof(uint64_t)

#endif // GBMEMORY_H
