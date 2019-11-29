#include "GBA.h"

#ifdef BKPT_SUPPORT
int oldreg[18];
char oldbuffer[10];
#endif

reg_pair reg[45];
memoryMap map[256];
bool ioReadable[0x400];
bool N_FLAG = 0;
bool C_FLAG = 0;
bool Z_FLAG = 0;
bool V_FLAG = 0;
bool armState = true;
bool armIrqEnable = true;
uint32_t armNextPC = 0x00000000;
int armMode = 0x1f;
uint32_t stop = 0x08000568;

uint8_t* bios = 0;
uint8_t* rom = 0;
uint8_t* internalRAM = 0;
uint8_t* workRAM = 0;
uint8_t* paletteRAM = 0;
uint8_t* vram = 0;
uint8_t* oam = 0;
uint8_t* ioMem = 0;
pixFormat* pix = 0;

uint16_t P1 = 0xFFFF;
uint16_t IE = 0x0000;
uint16_t IF = 0x0000;
uint16_t IME = 0x0000;
