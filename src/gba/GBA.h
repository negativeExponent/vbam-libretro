#ifndef GBA_H
#define GBA_H

#include "../common/Types.h"
#include "../System.h"
#include "../Util.h"

const uint64_t TICKS_PER_SECOND = 16777216;

#define SAVE_GAME_VERSION_1 1
#define SAVE_GAME_VERSION_2 2
#define SAVE_GAME_VERSION_3 3
#define SAVE_GAME_VERSION_4 4
#define SAVE_GAME_VERSION_5 5
#define SAVE_GAME_VERSION_6 6
#define SAVE_GAME_VERSION_7 7
#define SAVE_GAME_VERSION_8 8
#define SAVE_GAME_VERSION_9 9
#define SAVE_GAME_VERSION_10 10
#define SAVE_GAME_VERSION SAVE_GAME_VERSION_10

// register definitions
#define COMM_SIODATA32_L 0x120 // Lower 16bit on Normal mode
#define COMM_SIODATA32_H 0x122 // Higher 16bit on Normal mode
#define COMM_SIOCNT 0x128
#define COMM_SIODATA8 0x12a // 8bit on Normal/UART mode, (up to 4x8bit with FIFO)
#define COMM_SIOMLT_SEND 0x12a // SIOMLT_SEND (16bit R/W) on MultiPlayer mode (local outgoing)
#define COMM_SIOMULTI0 0x120 // SIOMULTI0 (16bit) on MultiPlayer mode (Parent/Master)
#define COMM_SIOMULTI1 0x122 // SIOMULTI1 (16bit) on MultiPlayer mode (Child1/Slave1)
#define COMM_SIOMULTI2 0x124 // SIOMULTI2 (16bit) on MultiPlayer mode (Child2/Slave2)
#define COMM_SIOMULTI3 0x126 // SIOMULTI3 (16bit) on MultiPlayer mode (Child3/Slave3)
#define COMM_RCNT 0x134 // SIO Mode (4bit data) on GeneralPurpose mode
#define COMM_IR 0x136 // Infrared Register (16bit) 1bit data at a time(LED On/Off)?
#define COMM_JOYCNT 0x140
#define COMM_JOY_RECV_L 0x150 // Send/Receive 8bit Lower first then 8bit Higher
#define COMM_JOY_RECV_H 0x152
#define COMM_JOY_TRANS_L 0x154 // Send/Receive 8bit Lower first then 8bit Higher
#define COMM_JOY_TRANS_H 0x156
#define COMM_JOYSTAT 0x158 // Send/Receive 8bit lower only

enum gba_geometry
{
    GBA_WIDTH  = 240,
    GBA_HEIGHT = 160
};

enum gba_savetype
{
    GBA_SAVE_AUTO          = 0,
    GBA_SAVE_EEPROM        = 1,
    GBA_SAVE_SRAM          = 2,
    GBA_SAVE_FLASH         = 3,
    GBA_SAVE_EEPROM_SENSOR = 4,
    GBA_SAVE_NONE          = 5
};

enum gba_savetype_size
{
    SIZE_SRAM       = 32768,
    SIZE_FLASH512   = 65536,
    SIZE_FLASH1M    = 131072,
    SIZE_EEPROM_512 = 512,
    SIZE_EEPROM_8K  = 8192
};

enum gba_memory_size
{
    SIZE_ROM   = 0x2000000,
    SIZE_BIOS  = 0x0004000,
    SIZE_IRAM  = 0x0008000,
    SIZE_WRAM  = 0x0040000,
    SIZE_PRAM  = 0x0000400,
    SIZE_VRAM  = 0x0020000,
    SIZE_OAM   = 0x0000400,
    SIZE_IOMEM = 0x0000400,
    SIZE_PIX = (4 * GBA_WIDTH * GBA_HEIGHT)
};

typedef struct {
    uint8_t* address;
    uint32_t mask;
#ifdef BKPT_SUPPORT
    uint8_t* breakPoints;
    uint8_t* searchMatch;
    uint8_t* trace;
    uint32_t size;
#endif
} memoryMap;

typedef union {
    struct {
#ifdef MSB_FIRST
        uint8_t B3;
        uint8_t B2;
        uint8_t B1;
        uint8_t B0;
#else
        uint8_t B0;
        uint8_t B1;
        uint8_t B2;
        uint8_t B3;
#endif
    } B;
    struct {
#ifdef MSB_FIRST
        uint16_t W1;
        uint16_t W0;
#else
        uint16_t W0;
        uint16_t W1;
#endif
    } W;
#ifdef MSB_FIRST
    volatile uint32_t I;
#else
    uint32_t I;
#endif
} reg_pair;

struct gba_timers_t
{
    struct timers_t
    {
        uint16_t Value;
        bool On;
        int Ticks;
        int Reload;
        int ClockReload;
    } tm[4];
    uint8_t OnOffDelay;
};

struct gba_dma_t
{
    uint32_t Source;
    uint32_t Dest;
};

extern gba_dma_t dma[4];
extern gba_timers_t timers;
extern memoryMap map[256];
extern uint8_t biosProtected[4];
extern void (*cpuSaveGameFunc)(uint32_t, uint8_t);
extern bool cpuSramEnabled;
extern bool cpuFlashEnabled;
extern bool cpuEEPROMEnabled;
extern bool cpuEEPROMSensorEnabled;

#ifdef BKPT_SUPPORT
extern uint8_t freezeWorkRAM[0x40000];
extern uint8_t freezeInternalRAM[0x8000];
extern uint8_t freezeVRAM[0x18000];
extern uint8_t freezeOAM[0x400];
extern uint8_t freezePRAM[0x400];
extern bool debugger_last;
extern int oldreg[18];
extern char oldbuffer[10];
extern bool debugger;
#endif

void CPUCleanUp();
void CPUUpdateRender();
void CPUUpdateRenderBuffers(bool);
bool CPUReadState(const uint8_t*, unsigned);
unsigned int CPUWriteState(uint8_t* data, unsigned int size);
int CPULoadRom(const char*);
int CPULoadRomData(const char* data, int size);
void doMirroring(bool);
void CPUUpdateRegister(uint32_t, uint16_t);
void applyTimer();
void CPUInit(const char*, bool);
void SetSaveType(int st);
void CPUReset();
void CPULoop(int);
void CPUCheckDMA(int, int);

const char* GetLoadDotCodeFile();
const char* GetSaveDotCodeFile();
void SetLoadDotCodeFile(const char* szFile);
void SetSaveDotCodeFile(const char* szFile);

extern struct EmulatedSystem GBASystem;

#define R13_IRQ 18
#define R14_IRQ 19
#define SPSR_IRQ 20
#define R13_USR 26
#define R14_USR 27
#define R13_SVC 28
#define R14_SVC 29
#define SPSR_SVC 30
#define R13_ABT 31
#define R14_ABT 32
#define SPSR_ABT 33
#define R13_UND 34
#define R14_UND 35
#define SPSR_UND 36
#define R8_FIQ 37
#define R9_FIQ 38
#define R10_FIQ 39
#define R11_FIQ 40
#define R12_FIQ 41
#define R13_FIQ 42
#define R14_FIQ 43
#define SPSR_FIQ 44

#include "Cheats.h"
#include "EEprom.h"
#include "Flash.h"
#include "Globals.h"

#endif // GBA_H
