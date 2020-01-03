#ifndef GB_MEMORY_H
#define GB_MEMORY_H

uint8_t gbReadMemory(uint16_t address);
void gbWriteMemory(uint16_t address, uint8_t value);

void gbCompareLYToLYC();
void gbCopyMemory(uint16_t d, uint16_t s, int count);
void gbDoHdma();

#endif // GB_MEMORY_H