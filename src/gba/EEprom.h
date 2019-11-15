#ifndef EEPROM_H
#define EEPROM_H

#include "../common/Types.h"

extern void eepromSaveGame(uint8_t*& data);
extern void eepromReadGame(const uint8_t*& data, int version);
extern uint8_t eepromData[0x2000];
extern int eepromRead(void);
extern void eepromWrite(uint8_t value);
extern void eepromInit();
extern void eepromReset();
extern bool eepromInUse;
extern int eepromSize;

#define EEPROM_IDLE 0
#define EEPROM_READADDRESS 1
#define EEPROM_READDATA 2
#define EEPROM_READDATA2 3
#define EEPROM_WRITEDATA 4

#endif // EEPROM_H
