#ifndef GBAINLINE_H
#define GBAINLINE_H

#include "../System.h"
#include "../common/Port.h"
#include "GBAcpu.h"
#include "RTC.h"
#include "Sound.h"

extern bool stopState;
extern bool holdState;
extern int holdType;
extern int cpuNextEvent;
extern bool cpuSramEnabled;
extern bool cpuFlashEnabled;
extern bool cpuEEPROMEnabled;
extern bool cpuEEPROMSensorEnabled;
extern bool cpuDmaHack;
extern uint32_t cpuDmaLast;
extern int cpuTotalTicks;

uint32_t CPUReadMemory(uint32_t);
uint32_t CPUReadHalfWord(uint32_t);
int16_t CPUReadHalfWordSigned(uint32_t);
uint8_t CPUReadByte(uint32_t);

void CPUWriteMemory(uint32_t, uint32_t);
void  CPUWriteHalfWord(uint32_t, uint16_t);
void  CPUWriteByte(uint32_t, uint8_t);

bool GBAMemoryInit(void);
void GBAMemoryCleanup(void);

void CPUCompareVCOUNT(void);

#define CPUReadByteQuick(addr) map[(addr) >> 24].address[(addr)&map[(addr) >> 24].mask]
#define CPUReadHalfWordQuick(addr) READ16LE(((uint16_t*)&map[(addr) >> 24].address[(addr)&map[(addr) >> 24].mask]))
#define CPUReadMemoryQuick(addr) READ32LE(((uint32_t*)&map[(addr) >> 24].address[(addr)&map[(addr) >> 24].mask]))

#endif // GBAINLINE_H

