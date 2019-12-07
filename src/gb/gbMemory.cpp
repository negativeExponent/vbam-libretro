#include <stdlib.h>

#include "../System.h"
#include "../common/Port.h"
#include "NLS.h"
#include "Util.h"
#include "gb.h"
#include "gbConstants.h"
#include "gbGlobals.h"
#include "gbMemory.h"

static uint8_t gbCheatingDevice = 0; // 1 = GS, 2 = GG
static uint8_t gbRamFill = 0xff;
static uint8_t gbDaysinMonth[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
static const uint8_t gbDisabledRam[8] = { 0x80, 0xff, 0xf0, 0x00, 0x30, 0xbf, 0xbf, 0xbf };

static void MBCswitchROMBanks(int address)
{
    // Bank 0
    gbMemoryMap[GB_MEM_CART_BANK0 + 0] = &gbRom[address + 0x0000];
    gbMemoryMap[GB_MEM_CART_BANK0 + 1] = &gbRom[address + 0x1000];
    gbMemoryMap[GB_MEM_CART_BANK0 + 2] = &gbRom[address + 0x2000];
    gbMemoryMap[GB_MEM_CART_BANK0 + 3] = &gbRom[address + 0x3000];

    // Bank 1
    gbMemoryMap[GB_MEM_CART_BANK1 + 0] = &gbRom[address + 0x4000];
    gbMemoryMap[GB_MEM_CART_BANK1 + 1] = &gbRom[address + 0x5000];
    gbMemoryMap[GB_MEM_CART_BANK1 + 2] = &gbRom[address + 0x6000];
    gbMemoryMap[GB_MEM_CART_BANK1 + 3] = &gbRom[address + 0x7000];
}

static void MBCswitchROMBank0(int address)
{
    gbMemoryMap[GB_MEM_CART_BANK0 + 0] = &gbRom[address + 0x0000];
    gbMemoryMap[GB_MEM_CART_BANK0 + 1] = &gbRom[address + 0x1000];
    gbMemoryMap[GB_MEM_CART_BANK0 + 2] = &gbRom[address + 0x2000];
    gbMemoryMap[GB_MEM_CART_BANK0 + 3] = &gbRom[address + 0x3000];
}

static void MBCswitchROMBank1(int address)
{
    gbMemoryMap[GB_MEM_CART_BANK1 + 0] = &gbRom[address + 0x0000];
    gbMemoryMap[GB_MEM_CART_BANK1 + 1] = &gbRom[address + 0x1000];
    gbMemoryMap[GB_MEM_CART_BANK1 + 2] = &gbRom[address + 0x2000];
    gbMemoryMap[GB_MEM_CART_BANK1 + 3] = &gbRom[address + 0x3000];
}

static void MBCswitchRAMBank(int address)
{
    gbMemoryMap[GB_MEM_EXTERNAL_RAM + 0] = &gbRam[address + 0x0000];
    gbMemoryMap[GB_MEM_EXTERNAL_RAM + 1] = &gbRam[address + 0x1000];
}

rtcData_t rtcData = {
    0, // timer seconds
    0, // timer minutes
    0, // timer hours
    0, // timer days
    0, // timer months
    0, // timer years
    0, // timer control
    0, // timer latched seconds
    0, // timer latched minutes
    0, // timer latched hours
    0, // timer latched days
    0, // timer latched months
    0, // timer latched years
    0, // timer latched control
    {(time_t)-1} // last time
};

mapperMBC1 gbDataMBC1 = {
    0, // RAM enable
    1, // ROM bank
    0, // RAM bank
    0, // memory model
    0, // ROM high address
    0, // RAM address
    0 // Rom Bank 0 remapping
};

// MBC1 ROM write registers
void mapperMBC1ROM(uint16_t address, uint8_t value)
{
    int tmpAddress = 0;

    switch (address & 0x6000) {
    case 0x0000: // RAM enable register
        gbDataMBC1.mapperRAMEnable = ((value & 0x0a) == 0x0a ? 1 : 0);
        break;
    case 0x2000: // ROM bank select
        //    value = value & 0x1f;
        if ((value == 1) && (address == 0x2100))
            gbDataMBC1.mapperRomBank0Remapping = 1;

        if ((value & 0x1f) == 0)
            value += 1;
        if (value == gbDataMBC1.mapperROMBank)
            break;

        tmpAddress = value << 14;

        // check current model
        if (gbDataMBC1.mapperRomBank0Remapping == 3) {
            tmpAddress = (value & 0xf) << 14;
            tmpAddress |= (gbDataMBC1.mapperROMHighAddress & 3) << 18;
        } else if (gbDataMBC1.mapperMemoryModel == 0) {
            // model is 16/8, so we have a high address in use
            tmpAddress |= (gbDataMBC1.mapperROMHighAddress & 3) << 19;
        }

        tmpAddress &= gbRomSizeMask;
        gbDataMBC1.mapperROMBank = value;
        MBCswitchROMBank1(tmpAddress);
        break;
    case 0x4000: // RAM bank select
        if (gbDataMBC1.mapperMemoryModel == 1) {
            if (!gbRamSize) {
                if (gbDataMBC1.mapperRomBank0Remapping == 3) {
                    gbDataMBC1.mapperROMHighAddress = value & 0x03;
                    tmpAddress = (gbDataMBC1.mapperROMHighAddress) << 18;
                    tmpAddress &= gbRomSizeMask;
                    MBCswitchROMBanks(tmpAddress);
                } else
                    gbDataMBC1.mapperRomBank0Remapping = 0;
            }
            // 4/32 model, RAM bank switching provided
            value = value & 0x03;
            if (value == gbDataMBC1.mapperRAMBank)
                break;
            tmpAddress = value << 13;
            tmpAddress &= gbRamSizeMask;
            if (gbRamSize) {
                MBCswitchRAMBank(tmpAddress);
            }
            gbDataMBC1.mapperRAMBank = value;
            gbDataMBC1.mapperRAMAddress = tmpAddress;

            if (gbDataMBC1.mapperRomBank0Remapping != 3)
                gbDataMBC1.mapperROMHighAddress = 0;
        } else {
            // 16/8, set the high address
            gbDataMBC1.mapperROMHighAddress = value & 0x03;
            tmpAddress = gbDataMBC1.mapperROMBank << 14;
            tmpAddress |= (gbDataMBC1.mapperROMHighAddress) << 19;
            tmpAddress &= gbRomSizeMask;
            MBCswitchROMBank1(tmpAddress);
            if (gbRamSize) {
                MBCswitchRAMBank(0);
            }

            gbDataMBC1.mapperRAMBank = 0;
        }
        break;
    case 0x6000: // memory model select
        gbDataMBC1.mapperMemoryModel = value & 1;

        if (gbDataMBC1.mapperMemoryModel == 1) {
            // 4/32 model, RAM bank switching provided

            value = gbDataMBC1.mapperRAMBank & 0x03;
            tmpAddress = value << 13;
            tmpAddress &= gbRamSizeMask;
            if (gbRamSize) {
                MBCswitchRAMBank(gbDataMBC1.mapperRAMAddress);
                gbDataMBC1.mapperRomBank0Remapping = 0;
            } else
                gbDataMBC1.mapperRomBank0Remapping |= 2;

            gbDataMBC1.mapperRAMBank = value;
            gbDataMBC1.mapperRAMAddress = tmpAddress;

            tmpAddress = gbDataMBC1.mapperROMBank << 14;

            tmpAddress &= gbRomSizeMask;
            MBCswitchROMBank1(tmpAddress);

        } else {
            // 16/8, set the high address

            tmpAddress = gbDataMBC1.mapperROMBank << 14;
            tmpAddress |= (gbDataMBC1.mapperROMHighAddress) << 19;
            tmpAddress &= gbRomSizeMask;
            MBCswitchROMBank1(tmpAddress);
            if (gbRamSize) {
                MBCswitchRAMBank(0);
            }
        }
        break;
    }
}

// MBC1 RAM write
void mapperMBC1RAM(uint16_t address, uint8_t value)
{
    if (gbDataMBC1.mapperRAMEnable) {
        if (gbRamSize) {
            gbMemoryMap[address >> 12][address & 0x0fff] = value;
            systemSaveUpdateCounter = SYSTEM_SAVE_UPDATED;
        }
    }
}

// MBC1 read RAM
uint8_t mapperMBC1ReadRAM(uint16_t address)
{

    if (gbDataMBC1.mapperRAMEnable)
        return gbMemoryMap[address >> 12][address & 0x0fff];

    if (!genericflashcardEnable)
        return 0xff;
    else if ((address & 0x1000) >= 0x1000) {
        // The value returned when reading RAM while it's disabled
        // is constant, exept for the GBASP hardware.
        // (actually, is the address that read is out of the ROM, the returned value if 0xff...)
        if (PC.W >= 0xff80)
            return 0xff;
        else if ((gbHardware & 0x08) && (gbGBCColorType == 2)) {
            if (address & 1)
                return 0xfb;
            else
                return 0x7a;
        } else
            return 0x0a;
    } else
        return gbDisabledRam[address & 7];
}

void memoryUpdateMapMBC1()
{
    int tmpAddress = gbDataMBC1.mapperROMBank << 14;

    // check current model
    if (gbDataMBC1.mapperRomBank0Remapping == 3) {
        tmpAddress = (gbDataMBC1.mapperROMHighAddress & 3) << 18;
        tmpAddress &= gbRomSizeMask;
        MBCswitchROMBank0(tmpAddress);

        tmpAddress |= (gbDataMBC1.mapperROMBank & 0xf) << 14;
        MBCswitchROMBank1(tmpAddress);
    } else {
        if (gbDataMBC1.mapperMemoryModel == 0) {
            // model is 16/8, so we have a high address in use
            tmpAddress |= (gbDataMBC1.mapperROMHighAddress & 3) << 19;
        }

        tmpAddress &= gbRomSizeMask;
        MBCswitchROMBank1(tmpAddress);
    }

    if (gbRamSize) {
        if (gbDataMBC1.mapperMemoryModel == 1) {
            MBCswitchRAMBank(gbDataMBC1.mapperRAMAddress);
        } else {
            MBCswitchRAMBank(0);
        }
    }
}

mapperMBC2 gbDataMBC2 = {
    0, // RAM enable
    1 // ROM bank
};

// MBC2 ROM write registers
void mapperMBC2ROM(uint16_t address, uint8_t value)
{
    switch (address & 0x6000) {
    case 0x0000: // RAM enable
        if (!(address & 0x0100)) {
            gbDataMBC2.mapperRAMEnable = (value & 0x0f) == 0x0a;
        }
        break;
    case 0x2000: // ROM bank select
        if (address & 0x0100) {
            value &= 0x0f;

            if (value == 0)
                value = 1;
            if (gbDataMBC2.mapperROMBank != value) {
                gbDataMBC2.mapperROMBank = value;

                int tmpAddress = value << 14;

                tmpAddress &= gbRomSizeMask;

                MBCswitchROMBank1(tmpAddress);
            }
        }
        break;
    }
}

// MBC2 RAM write
void mapperMBC2RAM(uint16_t address, uint8_t value)
{
    if (gbDataMBC2.mapperRAMEnable) {
        if (gbRamSize && address < 0xa200) {
            gbMemoryMap[address >> 12][address & 0x0fff] = value;
            systemSaveUpdateCounter = SYSTEM_SAVE_UPDATED;
        }
    }
}

void memoryUpdateMapMBC2()
{
    int tmpAddress = gbDataMBC2.mapperROMBank << 14;

    tmpAddress &= gbRomSizeMask;

    MBCswitchROMBank1(tmpAddress);
}

mapperMBC3 gbDataMBC3 = {
    0, // RAM enable
    1, // ROM bank
    0, // RAM bank
    0, // RAM address
    0, // timer clock latch
    0, // timer clock register
};

void memoryUpdateMBC3Clock()
{
    time_t now = time(NULL);
    time_t diff = now - rtcData.mapperLastTime;
    if (diff > 0) {
        // update the clock according to the last update time
        rtcData.mapperSeconds += (int)(diff % 60);
        if (rtcData.mapperSeconds > 59) {
            rtcData.mapperSeconds -= 60;
            rtcData.mapperMinutes++;
        }

        diff /= 60;

        rtcData.mapperMinutes += (int)(diff % 60);
        if (rtcData.mapperMinutes > 59) {
            rtcData.mapperMinutes -= 60;
            rtcData.mapperHours++;
        }

        diff /= 60;

        rtcData.mapperHours += (int)(diff % 24);
        if (rtcData.mapperHours > 23) {
            rtcData.mapperHours -= 24;
            rtcData.mapperDays++;
        }
        diff /= 24;

        rtcData.mapperDays += (int)(diff & 0xffffffff);
        if (rtcData.mapperDays > 255) {
            if (rtcData.mapperDays > 511) {
                rtcData.mapperDays %= 512;
                rtcData.mapperControl |= 0x80;
            }
            rtcData.mapperControl = (rtcData.mapperControl & 0xfe) | (rtcData.mapperDays > 255 ? 1 : 0);
        }
    }
    rtcData.mapperLastTime = now;
}

// MBC3 ROM write registers
void mapperMBC3ROM(uint16_t address, uint8_t value)
{
    int tmpAddress = 0;

    switch (address & 0x6000) {
    case 0x0000: // RAM enable register
        gbDataMBC3.mapperRAMEnable = ((value & 0x0a) == 0x0a ? 1 : 0);
        break;
    case 0x2000: // ROM bank select
        value = value & 0x7f;
        if (value == 0)
            value = 1;
        if (value == gbDataMBC3.mapperROMBank)
            break;

        tmpAddress = value << 14;

        tmpAddress &= gbRomSizeMask;
        gbDataMBC3.mapperROMBank = value;
        MBCswitchROMBank1(tmpAddress);

        break;
    case 0x4000: // RAM bank select
        if (value < 8) {
            if (value == gbDataMBC3.mapperRAMBank)
                break;
            tmpAddress = value << 13;
            tmpAddress &= gbRamSizeMask;
            MBCswitchRAMBank(tmpAddress);
            gbDataMBC3.mapperRAMBank = value;
            gbDataMBC3.mapperRAMAddress = tmpAddress;
        } else {
            if (gbRTCPresent && gbDataMBC3.mapperRAMEnable) {
                gbDataMBC3.mapperRAMBank = -1;

                gbDataMBC3.mapperClockRegister = value;
            }
        }
        break;
    case 0x6000: // clock latch
        if (gbRTCPresent) {
            if (gbDataMBC3.mapperClockLatch == 0 && value == 1) {
                memoryUpdateMBC3Clock();
                rtcData.mapperLSeconds = rtcData.mapperSeconds;
                rtcData.mapperLMinutes = rtcData.mapperMinutes;
                rtcData.mapperLHours = rtcData.mapperHours;
                rtcData.mapperLDays = rtcData.mapperDays;
                rtcData.mapperLControl = rtcData.mapperControl;
            }
            if (value == 0x00 || value == 0x01)
                gbDataMBC3.mapperClockLatch = value;
        }
        break;
    }
}

// MBC3 RAM write
void mapperMBC3RAM(uint16_t address, uint8_t value)
{
    if (gbDataMBC3.mapperRAMEnable) {
        if (gbDataMBC3.mapperRAMBank >= 0) {
            if (gbRamSize) {
                gbMemoryMap[address >> 12][address & 0x0fff] = value;
                systemSaveUpdateCounter = SYSTEM_SAVE_UPDATED;
            }
        } else if (gbRTCPresent) {
            time(&rtcData.mapperLastTime);
            switch (gbDataMBC3.mapperClockRegister) {
            case 0x08:
                rtcData.mapperSeconds = value;
                break;
            case 0x09:
                rtcData.mapperMinutes = value;
                break;
            case 0x0a:
                rtcData.mapperHours = value;
                break;
            case 0x0b:
                rtcData.mapperDays = value;
                break;
            case 0x0c:
                if (rtcData.mapperControl & 0x80)
                    rtcData.mapperControl = 0x80 | value;
                else
                    rtcData.mapperControl = value;
                break;
            }
        }
    }
}

// MBC3 read RAM
uint8_t mapperMBC3ReadRAM(uint16_t address)
{
    if (gbDataMBC3.mapperRAMEnable) {
        if (gbDataMBC3.mapperRAMBank >= 0) {
            return gbMemoryMap[address >> 12][address & 0x0fff];
        } else if (gbRTCPresent) {
            switch (gbDataMBC3.mapperClockRegister) {
            case 0x08:
                return rtcData.mapperLSeconds;
                break;
            case 0x09:
                return rtcData.mapperLMinutes;
                break;
            case 0x0a:
                return rtcData.mapperLHours;
                break;
            case 0x0b:
                return rtcData.mapperLDays;
                break;
            case 0x0c:
                return rtcData.mapperLControl;
            }
        }
    }

    if (!genericflashcardEnable)
        return 0xff;
    else if ((address & 0x1000) >= 0x1000) {
        // The value returned when reading RAM while it's disabled
        // is constant, exept for the GBASP hardware.
        // (actually, is the address that read is out of the ROM, the returned value if 0xff...)
        if (PC.W >= 0xff80)
            return 0xff;
        else if ((gbHardware & 0x08) && (gbGBCColorType == 2)) {
            if (address & 1)
                return 0xfb;
            else
                return 0x7a;
        } else
            return 0x0a;
    } else
        return gbDisabledRam[address & 7];
}

void memoryUpdateMapMBC3()
{
    int tmpAddress = gbDataMBC3.mapperROMBank << 14;

    tmpAddress &= gbRomSizeMask;

    MBCswitchROMBank1(tmpAddress);

    if (gbDataMBC3.mapperRAMBank >= 0 && gbRamSize) {
        tmpAddress = gbDataMBC3.mapperRAMBank << 13;
        tmpAddress &= gbRamSizeMask;
        MBCswitchRAMBank(tmpAddress);
    }
}

mapperMBC5 gbDataMBC5 = {
    0, // RAM enable
    1, // ROM bank
    0, // RAM bank
    0, // ROM high address
    0, // RAM address
    0 // is rumble cartridge?
};

// MBC5 ROM write registers
void mapperMBC5ROM(uint16_t address, uint8_t value)
{
    int tmpAddress = 0;

    switch (address & 0x6000) {
    case 0x0000: // RAM enable register
        gbDataMBC5.mapperRAMEnable = ((value & 0x0a) == 0x0a ? 1 : 0);
        break;
    case 0x2000: // ROM bank select

        if (address < 0x3000) {
            value = value & 0xff;
            if (value == gbDataMBC5.mapperROMBank)
                break;

            tmpAddress = (value << 14) | (gbDataMBC5.mapperROMHighAddress << 22);

            tmpAddress &= gbRomSizeMask;
            gbDataMBC5.mapperROMBank = value;
            MBCswitchROMBank1(tmpAddress);

        } else {
            value = value & 1;
            if (value == gbDataMBC5.mapperROMHighAddress)
                break;

            tmpAddress = (gbDataMBC5.mapperROMBank << 14) | (value << 22);

            tmpAddress &= gbRomSizeMask;
            gbDataMBC5.mapperROMHighAddress = value;
            MBCswitchROMBank1(tmpAddress);
        }
        break;
    case 0x4000: // RAM bank select
        if (gbDataMBC5.isRumbleCartridge) {
            systemCartridgeRumble(value & 8);
            value &= 0x07;
        } else
            value &= 0x0f;
        if (value == gbDataMBC5.mapperRAMBank)
            break;
        tmpAddress = value << 13;
        tmpAddress &= gbRamSizeMask;
        if (gbRamSize) {
            MBCswitchRAMBank(tmpAddress);

            gbDataMBC5.mapperRAMBank = value;
            gbDataMBC5.mapperRAMAddress = tmpAddress;
        }
        break;
    }
}

// MBC5 RAM write
void mapperMBC5RAM(uint16_t address, uint8_t value)
{
    if (gbDataMBC5.mapperRAMEnable) {
        if (gbRamSize) {
            gbMemoryMap[address >> 12][address & 0x0fff] = value;
            systemSaveUpdateCounter = SYSTEM_SAVE_UPDATED;
        }
    }
}

// MBC5 read RAM
uint8_t mapperMBC5ReadRAM(uint16_t address)
{

    if (gbDataMBC5.mapperRAMEnable)
        return gbMemoryMap[address >> 12][address & 0x0fff];

    if (!genericflashcardEnable)
        return 0xff;
    else if ((address & 0x1000) >= 0x1000) {
        // The value returned when reading RAM while it's disabled
        // is constant, exept for the GBASP hardware.
        // (actually, is the address that read is out of the ROM, the returned value if 0xff...)
        if (PC.W >= 0xff80)
            return 0xff;
        else if ((gbHardware & 0x08) && (gbGBCColorType == 2)) {
            if (address & 1)
                return 0xfb;
            else
                return 0x7a;
        } else
            return 0x0a;
    } else
        return gbDisabledRam[address & 7];
}

void memoryUpdateMapMBC5()
{
    int tmpAddress = (gbDataMBC5.mapperROMBank << 14) | (gbDataMBC5.mapperROMHighAddress << 22);

    tmpAddress &= gbRomSizeMask;
    MBCswitchROMBank1(tmpAddress);

    if (gbRamSize) {
        tmpAddress = gbDataMBC5.mapperRAMBank << 13;
        tmpAddress &= gbRamSizeMask;
        MBCswitchRAMBank(tmpAddress);
    }
}

mapperMBC7 gbDataMBC7 = {
    0, // RAM enable
    1, // ROM bank
    0, // RAM bank
    0, // RAM address
    0, // chip select
    0, // ??
    0, // mapper state
    0, // buffer for receiving serial data
    0, // idle state
    0, // count of bits received
    0, // command received
    0, // address received
    0, // write enable
    0, // value to return on ram
};

// MBC7 ROM write registers
void mapperMBC7ROM(uint16_t address, uint8_t value)
{
    int tmpAddress = 0;

    switch (address & 0x6000) {
    case 0x0000:
        break;
    case 0x2000: // ROM bank select
        value = value & 0x7f;
        if (value == 0)
            value = 1;

        if (value == gbDataMBC7.mapperROMBank)
            break;

        tmpAddress = (value << 14);

        tmpAddress &= gbRomSizeMask;
        gbDataMBC7.mapperROMBank = value;
        MBCswitchROMBank1(tmpAddress);
        break;
    case 0x4000: // RAM bank select/enable
        if (value < 8) {
            tmpAddress = (value & 3) << 13;
            tmpAddress &= gbRamSizeMask;
            gbMemoryMap[GB_MEM_EXTERNAL_RAM] = &gbMemory[0xa000];
            gbMemoryMap[GB_MEM_EXTERNAL_RAM + 1] = &gbMemory[0xb000];

            gbDataMBC7.mapperRAMBank = value;
            gbDataMBC7.mapperRAMAddress = tmpAddress;
            gbDataMBC7.mapperRAMEnable = 0;
        } else {
            gbDataMBC7.mapperRAMEnable = 0;
        }
        break;
    }
}

// MBC7 read RAM
uint8_t mapperMBC7ReadRAM(uint16_t address)
{
    switch (address & 0xa0f0) {
    case 0xa000:
    case 0xa010:
    case 0xa060:
    case 0xa070:
        return 0;
    case 0xa020:
        // sensor X low byte
        return systemGetSensorX() & 255;
    case 0xa030:
        // sensor X high byte
        return systemGetSensorX() >> 8;
    case 0xa040:
        // sensor Y low byte
        return systemGetSensorY() & 255;
    case 0xa050:
        // sensor Y high byte
        return systemGetSensorY() >> 8;
    case 0xa080:
        return gbDataMBC7.value;
    }

    if (!genericflashcardEnable)
        return 0xff;
    else if ((address & 0x1000) >= 0x1000) {
        // The value returned when reading RAM while it's disabled
        // is constant, exept for the GBASP hardware.
        // (actually, is the address that read is out of the ROM, the returned value if 0xff...)
        if (PC.W >= 0xff80)
            return 0xff;
        else if ((gbHardware & 0x08) && (gbGBCColorType == 2)) {
            if (address & 1)
                return 0xfb;
            else
                return 0x7a;
        } else
            return 0x0a;
    } else
        return gbDisabledRam[address & 7];
}

// MBC7 RAM write
void mapperMBC7RAM(uint16_t address, uint8_t value)
{
    if (address == 0xa080) {
        // special processing needed
        int oldCs = gbDataMBC7.cs, oldSk = gbDataMBC7.sk;

        gbDataMBC7.cs = value >> 7;
        gbDataMBC7.sk = (value >> 6) & 1;

        if (!oldCs && gbDataMBC7.cs) {
            if (gbDataMBC7.state == 5) {
                if (gbDataMBC7.writeEnable) {
                    gbMemory[0xa000 + gbDataMBC7.address * 2] = gbDataMBC7.buffer >> 8;
                    gbMemory[0xa000 + gbDataMBC7.address * 2 + 1] = gbDataMBC7.buffer & 0xff;
                    systemSaveUpdateCounter = SYSTEM_SAVE_UPDATED;
                }
                gbDataMBC7.state = 0;
                gbDataMBC7.value = 1;
            } else {
                gbDataMBC7.idle = true;
                gbDataMBC7.state = 0;
            }
        }

        if (!oldSk && gbDataMBC7.sk) {
            if (gbDataMBC7.idle) {
                if (value & 0x02) {
                    gbDataMBC7.idle = false;
                    gbDataMBC7.count = 0;
                    gbDataMBC7.state = 1;
                }
            } else {
                switch (gbDataMBC7.state) {
                case 1:
                    // receiving command
                    gbDataMBC7.buffer <<= 1;
                    gbDataMBC7.buffer |= (value & 0x02) ? 1 : 0;
                    gbDataMBC7.count++;
                    if (gbDataMBC7.count == 2) {
                        // finished receiving command
                        gbDataMBC7.state = 2;
                        gbDataMBC7.count = 0;
                        gbDataMBC7.code = gbDataMBC7.buffer & 3;
                    }
                    break;
                case 2:
                    // receive address
                    gbDataMBC7.buffer <<= 1;
                    gbDataMBC7.buffer |= (value & 0x02) ? 1 : 0;
                    gbDataMBC7.count++;
                    if (gbDataMBC7.count == 8) {
                        // finish receiving
                        gbDataMBC7.state = 3;
                        gbDataMBC7.count = 0;
                        gbDataMBC7.address = gbDataMBC7.buffer & 0xff;
                        if (gbDataMBC7.code == 0) {
                            if ((gbDataMBC7.address >> 6) == 0) {
                                gbDataMBC7.writeEnable = 0;
                                gbDataMBC7.state = 0;
                            } else if ((gbDataMBC7.address >> 6) == 3) {
                                gbDataMBC7.writeEnable = 1;
                                gbDataMBC7.state = 0;
                            }
                        }
                    }
                    break;
                case 3:
                    gbDataMBC7.buffer <<= 1;
                    gbDataMBC7.buffer |= (value & 0x02) ? 1 : 0;
                    gbDataMBC7.count++;

                    switch (gbDataMBC7.code) {
                    case 0:
                        if (gbDataMBC7.count == 16) {
                            if ((gbDataMBC7.address >> 6) == 0) {
                                gbDataMBC7.writeEnable = 0;
                                gbDataMBC7.state = 0;
                            } else if ((gbDataMBC7.address >> 6) == 1) {
                                if (gbDataMBC7.writeEnable) {
                                    for (int i = 0; i < 256; i++) {
                                        gbMemory[0xa000 + i * 2] = gbDataMBC7.buffer >> 8;
                                        gbMemory[0xa000 + i * 2 + 1] = gbDataMBC7.buffer & 0xff;
                                        systemSaveUpdateCounter = SYSTEM_SAVE_UPDATED;
                                    }
                                }
                                gbDataMBC7.state = 5;
                            } else if ((gbDataMBC7.address >> 6) == 2) {
                                if (gbDataMBC7.writeEnable) {
                                    for (int i = 0; i < 256; i++)
                                        WRITE16LE((uint16_t*)&gbMemory[0xa000 + i * 2], 0xffff);
                                    systemSaveUpdateCounter = SYSTEM_SAVE_UPDATED;
                                }
                                gbDataMBC7.state = 5;
                            } else if ((gbDataMBC7.address >> 6) == 3) {
                                gbDataMBC7.writeEnable = 1;
                                gbDataMBC7.state = 0;
                            }
                            gbDataMBC7.count = 0;
                        }
                        break;
                    case 1:
                        if (gbDataMBC7.count == 16) {
                            gbDataMBC7.count = 0;
                            gbDataMBC7.state = 5;
                            gbDataMBC7.value = 0;
                        }
                        break;
                    case 2:
                        if (gbDataMBC7.count == 1) {
                            gbDataMBC7.state = 4;
                            gbDataMBC7.count = 0;
                            gbDataMBC7.buffer = (gbMemory[0xa000 + gbDataMBC7.address * 2] << 8) | (gbMemory[0xa000 + gbDataMBC7.address * 2 + 1]);
                        }
                        break;
                    case 3:
                        if (gbDataMBC7.count == 16) {
                            gbDataMBC7.count = 0;
                            gbDataMBC7.state = 5;
                            gbDataMBC7.value = 0;
                            gbDataMBC7.buffer = 0xffff;
                        }
                        break;
                    }
                    break;
                }
            }
        }

        if (oldSk && !gbDataMBC7.sk) {
            if (gbDataMBC7.state == 4) {
                gbDataMBC7.value = (gbDataMBC7.buffer & 0x8000) ? 1 : 0;
                gbDataMBC7.buffer <<= 1;
                gbDataMBC7.count++;
                if (gbDataMBC7.count == 16) {
                    gbDataMBC7.count = 0;
                    gbDataMBC7.state = 0;
                }
            }
        }
    }
}

void memoryUpdateMapMBC7()
{
    int tmpAddress = (gbDataMBC7.mapperROMBank << 14);

    tmpAddress &= gbRomSizeMask;
    MBCswitchROMBank1(tmpAddress);
}

mapperHuC1 gbDataHuC1 = {
    0, // RAM enable
    1, // ROM bank
    0, // RAM bank
    0, // memory model
    0, // ROM high address
    0 // RAM address
};

// HuC1 ROM write registers
void mapperHuC1ROM(uint16_t address, uint8_t value)
{
    int tmpAddress = 0;

    switch (address & 0x6000) {
    case 0x0000: // RAM enable register
        gbDataHuC1.mapperRAMEnable = ((value & 0x0a) == 0x0a ? 1 : 0);
        break;
    case 0x2000: // ROM bank select
        value = value & 0x3f;
        if (value == 0)
            value = 1;
        if (value == gbDataHuC1.mapperROMBank)
            break;

        tmpAddress = value << 14;

        tmpAddress &= gbRomSizeMask;
        gbDataHuC1.mapperROMBank = value;
        MBCswitchROMBank1(tmpAddress);
        break;
    case 0x4000: // RAM bank select
        if (gbDataHuC1.mapperMemoryModel == 1) {
            // 4/32 model, RAM bank switching provided
            value = value & 0x03;
            if (value == gbDataHuC1.mapperRAMBank)
                break;
            tmpAddress = value << 13;
            tmpAddress &= gbRamSizeMask;
            MBCswitchRAMBank(tmpAddress);
            gbDataHuC1.mapperRAMBank = value;
            gbDataHuC1.mapperRAMAddress = tmpAddress;
        } else {
            // 16/8, set the high address
            gbDataHuC1.mapperROMHighAddress = value & 0x03;
            tmpAddress = gbDataHuC1.mapperROMBank << 14;
            tmpAddress |= (gbDataHuC1.mapperROMHighAddress) << 19;
            tmpAddress &= gbRomSizeMask;
            MBCswitchROMBank1(tmpAddress);
        }
        break;
    case 0x6000: // memory model select
        gbDataHuC1.mapperMemoryModel = value & 1;
        break;
    }
}

// HuC1 RAM write
void mapperHuC1RAM(uint16_t address, uint8_t value)
{
    if (gbDataHuC1.mapperRAMEnable) {
        if (gbRamSize) {
            gbMemoryMap[address >> 12][address & 0x0fff] = value;
            systemSaveUpdateCounter = SYSTEM_SAVE_UPDATED;
        }
    }
}

void memoryUpdateMapHuC1()
{
    int tmpAddress = gbDataHuC1.mapperROMBank << 14;

    tmpAddress &= gbRomSizeMask;

    MBCswitchROMBank1(tmpAddress);

    if (gbRamSize) {
        tmpAddress = gbDataHuC1.mapperRAMBank << 13;
        tmpAddress &= gbRamSizeMask;
        MBCswitchRAMBank(tmpAddress);
    }
}

mapperHuC3 gbDataHuC3 = {
    0, // RAM enable
    1, // ROM bank
    0, // RAM bank
    0, // RAM address
    0, // Address
    0, // RAM flag
    0, // RAM read value
    0, // Register 1
    0, // Register 2
    0, // Register 3
    0, // Register 4
    0, // Register 5
    0, // Register 6
    0, // Register 7
    0  // Register 8
};

// HuC3 ROM write registers
void mapperHuC3ROM(uint16_t address, uint8_t value)
{
    int tmpAddress = 0;

    switch (address & 0x6000) {
    case 0x0000: // RAM enable register
        gbDataHuC3.mapperRAMEnable = (value == 0x0a ? 1 : 0);
        gbDataHuC3.mapperRAMFlag = value;
        if (gbDataHuC3.mapperRAMFlag != 0x0a)
            gbDataHuC3.mapperRAMBank = -1;
        break;
    case 0x2000: // ROM bank select
        value = value & 0x7f;
        if (value == 0)
            value = 1;
        if (value == gbDataHuC3.mapperROMBank)
            break;

        tmpAddress = value << 14;

        tmpAddress &= gbRomSizeMask;
        gbDataHuC3.mapperROMBank = value;
        MBCswitchROMBank1(tmpAddress);
        break;
    case 0x4000: // RAM bank select
        value = value & 0x03;
        if (value == gbDataHuC3.mapperRAMBank)
            break;
        tmpAddress = value << 13;
        tmpAddress &= gbRamSizeMask;
        MBCswitchRAMBank(tmpAddress);
        gbDataHuC3.mapperRAMBank = value;
        gbDataHuC3.mapperRAMAddress = tmpAddress;
        break;
    case 0x6000: // nothing to do!
        break;
    }
}

// HuC3 read RAM
uint8_t mapperHuC3ReadRAM(uint16_t address)
{
    if (gbDataHuC3.mapperRAMFlag > 0x0b && gbDataHuC3.mapperRAMFlag < 0x0e) {
        if (gbDataHuC3.mapperRAMFlag != 0x0c)
            return 1;
        return gbDataHuC3.mapperRAMValue;
    } else
        return gbMemoryMap[address >> 12][address & 0x0fff];
}

// HuC3 RAM write
void mapperHuC3RAM(uint16_t address, uint8_t value)
{
    int* p;

    if (gbDataHuC3.mapperRAMFlag < 0x0b || gbDataHuC3.mapperRAMFlag > 0x0e) {
        if (gbDataHuC3.mapperRAMEnable) {
            if (gbRamSize) {
                gbMemoryMap[address >> 12][address & 0x0fff] = value;
                systemSaveUpdateCounter = SYSTEM_SAVE_UPDATED;
            }
        }
    } else {
        if (gbDataHuC3.mapperRAMFlag == 0x0b) {
            if (value == 0x62) {
                gbDataHuC3.mapperRAMValue = 1;
            } else {
                switch (value & 0xf0) {
                case 0x10:
                    p = &gbDataHuC3.mapperRegister2;
                    gbDataHuC3.mapperRAMValue = *(p + gbDataHuC3.mapperRegister1++);
                    if (gbDataHuC3.mapperRegister1 > 6)
                        gbDataHuC3.mapperRegister1 = 0;
                    break;
                case 0x30:
                    p = &gbDataHuC3.mapperRegister2;
                    *(p + gbDataHuC3.mapperRegister1++) = value & 0x0f;
                    if (gbDataHuC3.mapperRegister1 > 6)
                        gbDataHuC3.mapperRegister1 = 0;
                    gbDataHuC3.mapperAddress = (gbDataHuC3.mapperRegister6 << 24) | (gbDataHuC3.mapperRegister5 << 16) | (gbDataHuC3.mapperRegister4 << 8) | (gbDataHuC3.mapperRegister3 << 4) | (gbDataHuC3.mapperRegister2);
                    break;
                case 0x40:
                    gbDataHuC3.mapperRegister1 = (gbDataHuC3.mapperRegister1 & 0xf0) | (value & 0x0f);
                    gbDataHuC3.mapperRegister2 = (gbDataHuC3.mapperAddress & 0x0f);
                    gbDataHuC3.mapperRegister3 = ((gbDataHuC3.mapperAddress >> 4) & 0x0f);
                    gbDataHuC3.mapperRegister4 = ((gbDataHuC3.mapperAddress >> 8) & 0x0f);
                    gbDataHuC3.mapperRegister5 = ((gbDataHuC3.mapperAddress >> 16) & 0x0f);
                    gbDataHuC3.mapperRegister6 = ((gbDataHuC3.mapperAddress >> 24) & 0x0f);
                    gbDataHuC3.mapperRegister7 = 0;
                    gbDataHuC3.mapperRegister8 = 0;
                    gbDataHuC3.mapperRAMValue = 0;
                    break;
                case 0x50:
                    gbDataHuC3.mapperRegister1 = (gbDataHuC3.mapperRegister1 & 0x0f) | ((value << 4) & 0x0f);
                    break;
                default:
                    gbDataHuC3.mapperRAMValue = 1;
                    break;
                }
            }
        }
    }
}

void memoryUpdateMapHuC3()
{
    int tmpAddress = gbDataHuC3.mapperROMBank << 14;

    tmpAddress &= gbRomSizeMask;
    MBCswitchROMBank1(tmpAddress);

    if (gbRamSize) {
        tmpAddress = gbDataHuC3.mapperRAMBank << 13;
        tmpAddress &= gbRamSizeMask;
        MBCswitchRAMBank(tmpAddress);
    }
}

// TAMA5 (for Tamagotchi 3 (gb)).
// Very basic (and ugly :p) support, only rom bank switching is actually working...
mapperTAMA5 gbDataTAMA5 = {
    1, // RAM enable
    1, // ROM bank
    0, // RAM bank
    0, // RAM address
    0, // RAM Byte select
    0, // mapper command number
    0, // mapper last command;
    {
        0, // commands 0x0
        0, // commands 0x1
        0, // commands 0x2
        0, // commands 0x3
        0, // commands 0x4
        0, // commands 0x5
        0, // commands 0x6
        0, // commands 0x7
        0, // commands 0x8
        0, // commands 0x9
        0, // commands 0xa
        0, // commands 0xb
        0, // commands 0xc
        0, // commands 0xd
        0, // commands 0xe
        0 // commands 0xf
    },
    0, // register
    0, // timer clock latch
    0, // timer clock register
};

void memoryUpdateTAMA5Clock()
{
    if ((rtcData.mapperYears & 3) == 0)
        gbDaysinMonth[1] = 29;
    else
        gbDaysinMonth[1] = 28;

    time_t now = time(NULL);
    time_t diff = now - rtcData.mapperLastTime;
    if (diff > 0) {
        // update the clock according to the last update time
        rtcData.mapperSeconds += (int)(diff % 60);
        if (rtcData.mapperSeconds > 59) {
            rtcData.mapperSeconds -= 60;
            rtcData.mapperMinutes++;
        }

        diff /= 60;

        rtcData.mapperMinutes += (int)(diff % 60);
        if (rtcData.mapperMinutes > 59) {
            rtcData.mapperMinutes -= 60;
            rtcData.mapperHours++;
        }

        diff /= 60;

        rtcData.mapperHours += (int)(diff % 24);
        diff /= 24;
        if (rtcData.mapperHours > 23) {
            rtcData.mapperHours -= 24;
            diff++;
        }

        time_t days = diff;
        while (days) {
            rtcData.mapperDays++;
            days--;
            if (rtcData.mapperDays > gbDaysinMonth[rtcData.mapperMonths - 1]) {
                rtcData.mapperDays = 1;
                rtcData.mapperMonths++;
                if (rtcData.mapperMonths > 12) {
                    rtcData.mapperMonths = 1;
                    rtcData.mapperYears++;
                    if ((rtcData.mapperYears & 3) == 0)
                        gbDaysinMonth[1] = 29;
                    else
                        gbDaysinMonth[1] = 28;
                }
            }
        }
    }
    rtcData.mapperLastTime = now;
}

// TAMA5 RAM write
void mapperTAMA5RAM(uint16_t address, uint8_t value)
{
    if ((address & 0xffff) <= 0xa001) {
        switch (address & 1) {
        case 0: // 'Values' Register
        {
            value &= 0xf;
            gbDataTAMA5.mapperCommands[gbDataTAMA5.mapperCommandNumber] = value;
            gbMemoryMap[0xa][0] = value;

            /*        int test = gbDataTAMA5.mapperCommands[gbDataTAMA5.mapperCommandNumber & 0x0e] |
                                    (gbDataTAMA5.mapperCommands[(gbDataTAMA5.mapperCommandNumber & 0x0e) +1]<<4);*/

            if ((gbDataTAMA5.mapperCommandNumber & 0xe) == 0) // Read Command !!!
            {
                gbDataTAMA5.mapperROMBank = gbDataTAMA5.mapperCommands[0] | (gbDataTAMA5.mapperCommands[1] << 4);

                int tmpAddress = (gbDataTAMA5.mapperROMBank << 14);

                tmpAddress &= gbRomSizeMask;
                MBCswitchROMBank1(tmpAddress);

                gbDataTAMA5.mapperCommands[0x0f] = 0;
            } else if ((gbDataTAMA5.mapperCommandNumber & 0xe) == 4) {
                gbDataTAMA5.mapperCommands[0x0f] = 1;
                if (gbDataTAMA5.mapperCommandNumber == 4)
                    gbDataTAMA5.mapperCommands[5] = 0; // correct ?
            } else if ((gbDataTAMA5.mapperCommandNumber & 0xe) == 6) {
                gbDataTAMA5.mapperRamByteSelect = (gbDataTAMA5.mapperCommands[7] << 4) | (gbDataTAMA5.mapperCommands[6] & 0x0f);

                // Write Commands !!!
                if (gbDataTAMA5.mapperCommands[0x0f] && (gbDataTAMA5.mapperCommandNumber == 7)) {
                    int data = (gbDataTAMA5.mapperCommands[0x04] & 0x0f) | (gbDataTAMA5.mapperCommands[0x05] << 4);

                    // Not sure when the write command should reset...
                    // but it doesn't seem to matter.
                    // gbDataTAMA5.mapperCommands[0x0f] = 0;

                    if (gbDataTAMA5.mapperRamByteSelect == 0x8) // Timer stuff
                    {
                        switch (data & 0xf) {
                        case 0x7:
                            rtcData.mapperDays = ((rtcData.mapperDays) / 10) * 10 + (data >> 4);
                            break;
                        case 0x8:
                            rtcData.mapperDays = (rtcData.mapperDays % 10) + (data >> 4) * 10;
                            break;
                        case 0x9:
                            rtcData.mapperMonths = ((rtcData.mapperMonths) / 10) * 10 + (data >> 4);
                            break;
                        case 0xa:
                            rtcData.mapperMonths = (rtcData.mapperMonths % 10) + (data >> 4) * 10;
                            break;
                        case 0xb:
                            rtcData.mapperYears = ((rtcData.mapperYears) % 1000) + (data >> 4) * 1000;
                            break;
                        case 0xc:
                            rtcData.mapperYears = (rtcData.mapperYears % 100) + (rtcData.mapperYears / 1000) * 1000 + (data >> 4) * 100;
                            break;
                        default:
                            break;
                        }
                    } else if (gbDataTAMA5.mapperRamByteSelect == 0x18) // Timer stuff again
                    {
                        memoryUpdateTAMA5Clock();
                        rtcData.mapperLSeconds = rtcData.mapperSeconds;
                        rtcData.mapperLMinutes = rtcData.mapperMinutes;
                        rtcData.mapperLHours = rtcData.mapperHours;
                        rtcData.mapperLDays = rtcData.mapperDays;
                        rtcData.mapperLMonths = rtcData.mapperMonths;
                        rtcData.mapperLYears = rtcData.mapperYears;
                        rtcData.mapperLControl = rtcData.mapperControl;

                        int seconds = (rtcData.mapperLSeconds / 10) * 16 + rtcData.mapperLSeconds % 10;
                        int secondsL = (rtcData.mapperLSeconds % 10);
                        int secondsH = (rtcData.mapperLSeconds / 10);
                        int minutes = (rtcData.mapperLMinutes / 10) * 16 + rtcData.mapperLMinutes % 10;
                        int hours = (rtcData.mapperLHours / 10) * 16 + rtcData.mapperLHours % 10;
                        int DaysL = rtcData.mapperLDays % 10;
                        int DaysH = rtcData.mapperLDays / 10;
                        int MonthsL = rtcData.mapperLMonths % 10;
                        int MonthsH = rtcData.mapperLMonths / 10;
                        int Years3 = (rtcData.mapperLYears / 100) % 10;
                        int Years4 = (rtcData.mapperLYears / 1000);

                        switch (data & 0x0f) {
                        // I guess cases 0 and 1 are used for secondsL and secondsH
                        // so the game would update the timer values on screen when
                        // the seconds reset to 0... ?
                        case 0x0:
                            gbTAMA5ram[gbDataTAMA5.mapperRamByteSelect] = secondsL;
                            break;
                        case 0x1:
                            gbTAMA5ram[gbDataTAMA5.mapperRamByteSelect] = secondsH;
                            break;
                        case 0x7:
                            gbTAMA5ram[gbDataTAMA5.mapperRamByteSelect] = DaysL; // days low
                            break;
                        case 0x8:
                            gbTAMA5ram[gbDataTAMA5.mapperRamByteSelect] = DaysH; // days high
                            break;
                        case 0x9:
                            gbTAMA5ram[gbDataTAMA5.mapperRamByteSelect] = MonthsL; // month low
                            break;
                        case 0xa:
                            gbTAMA5ram[gbDataTAMA5.mapperRamByteSelect] = MonthsH; // month high
                            break;
                        case 0xb:
                            gbTAMA5ram[gbDataTAMA5.mapperRamByteSelect] = Years4; // years 4th digit
                            break;
                        case 0xc:
                            gbTAMA5ram[gbDataTAMA5.mapperRamByteSelect] = Years3; // years 3rd digit
                            break;
                        default:
                            break;
                        }

                        gbTAMA5ram[0x54] = seconds; // incorrect ? (not used by the game) ?
                        gbTAMA5ram[0x64] = minutes;
                        gbTAMA5ram[0x74] = hours;
                        gbTAMA5ram[0x84] = DaysH * 16 + DaysL; // incorrect ? (not used by the game) ?
                        gbTAMA5ram[0x94] = MonthsH * 16 + MonthsL; // incorrect ? (not used by the game) ?

                        time(&rtcData.mapperLastTime);

                        gbMemoryMap[0xa][0] = 1;
                    } else if (gbDataTAMA5.mapperRamByteSelect == 0x28) // Timer stuff again
                    {
                        if ((data & 0xf) == 0xb)
                            rtcData.mapperYears = ((rtcData.mapperYears >> 2) << 2) + (data & 3);
                    } else if (gbDataTAMA5.mapperRamByteSelect == 0x44) {
                        rtcData.mapperMinutes = (data / 16) * 10 + data % 16;
                    } else if (gbDataTAMA5.mapperRamByteSelect == 0x54) {
                        rtcData.mapperHours = (data / 16) * 10 + data % 16;
                    } else {
                        gbTAMA5ram[gbDataTAMA5.mapperRamByteSelect] = data;
                    }
                }
            }
        } break;
        case 1: // 'Commands' Register
        {
            gbMemoryMap[0xa][1] = gbDataTAMA5.mapperCommandNumber = value;

            // This should be only a 'is the flashrom ready ?' command.
            // However as I couldn't find any 'copy' command
            // (that seems to be needed for the saving system to work)
            // I put it there...
            if (value == 0x0a) {
                for (int i = 0; i < 0x10; i++)
                    for (int j = 0; j < 0x10; j++)
                        if (!(j & 2))
                            gbTAMA5ram[((i * 0x10) + j) | 2] = gbTAMA5ram[(i * 0x10) + j];
                // Enable this to see the content of the flashrom in 0xe000
                /*for (int k = 0; k<0x100; k++)
            gbMemoryMap[0xe][k] = gbTAMA5ram[k];*/

                gbMemoryMap[0xa][0] = gbDataTAMA5.mapperRAMEnable = 1;
            } else {
                if ((value & 0x0e) == 0x0c) {
                    gbDataTAMA5.mapperRamByteSelect = gbDataTAMA5.mapperCommands[6] | (gbDataTAMA5.mapperCommands[7] << 4);

                    uint8_t byte = gbTAMA5ram[gbDataTAMA5.mapperRamByteSelect];

                    gbMemoryMap[0xa][0] = (value & 1) ? byte >> 4 : byte & 0x0f;

                    gbDataTAMA5.mapperCommands[0x0f] = 0;
                }
            }
            break;
        }
        }
    } else {
        if (gbDataTAMA5.mapperRAMEnable) {
            if (gbDataTAMA5.mapperRAMBank != -1) {
                if (gbRamSize) {
                    gbMemoryMap[address >> 12][address & 0x0fff] = value;
                    systemSaveUpdateCounter = SYSTEM_SAVE_UPDATED;
                }
            }
        }
    }
}

// TAMA5 read RAM
uint8_t mapperTAMA5ReadRAM(uint16_t address)
{
    return gbMemoryMap[address >> 12][address & 0xfff];
}

void memoryUpdateMapTAMA5()
{
    int tmpAddress = (gbDataTAMA5.mapperROMBank << 14);

    tmpAddress &= gbRomSizeMask;
    MBCswitchROMBank1(tmpAddress);

    if (gbRamSize) {
        tmpAddress = 0 << 13;
        tmpAddress &= gbRamSizeMask;
        MBCswitchRAMBank(tmpAddress);
    }
}

// MMM01 Used in Momotarou collection (however the rom is corrupted)
mapperMMM01 gbDataMMM01 = {
    0, // RAM enable
    1, // ROM bank
    0, // RAM bank
    0, // memory model
    0, // ROM high address
    0, // RAM address
    0 // Rom Bank 0 remapping
};

// MMM01 ROM write registers
void mapperMMM01ROM(uint16_t address, uint8_t value)
{
    int tmpAddress = 0;

    switch (address & 0x6000) {
    case 0x0000: // RAM enable register
        gbDataMMM01.mapperRAMEnable = ((value & 0x0a) == 0x0a ? 1 : 0);
        break;
    case 0x2000: // ROM bank select
        //    value = value & 0x1f;
        if (value == 0)
            value = 1;
        if (value == gbDataMMM01.mapperROMBank)
            break;

        tmpAddress = value << 14;

        // check current model
        if (gbDataMMM01.mapperMemoryModel == 0) {
            // model is 16/8, so we have a high address in use
            tmpAddress |= (gbDataMMM01.mapperROMHighAddress) << 19;
        } else
            tmpAddress |= gbDataMMM01.mapperRomBank0Remapping << 18;

        tmpAddress &= gbRomSizeMask;
        gbDataMMM01.mapperROMBank = value;
        MBCswitchROMBank1(tmpAddress);
        break;
    case 0x4000: // RAM bank select
        if (gbDataMMM01.mapperMemoryModel == 1) {
            // 4/32 model, RAM bank switching provided
            value = value & 0x03;
            if (value == gbDataMBC1.mapperRAMBank)
                break;
            tmpAddress = value << 13;
            tmpAddress &= gbRamSizeMask;
            MBCswitchRAMBank(tmpAddress);
            gbDataMMM01.mapperRAMBank = value;
            gbDataMMM01.mapperRAMAddress = tmpAddress;
        } else {
            // 16/8, set the high address
            gbDataMMM01.mapperROMHighAddress = value & 0x03;
            tmpAddress = gbDataMMM01.mapperROMBank << 14;
            tmpAddress |= (gbDataMMM01.mapperROMHighAddress) << 19;
            tmpAddress &= gbRomSizeMask;
            MBCswitchROMBank1(tmpAddress);

            gbDataMMM01.mapperRomBank0Remapping = ((value << 1) | (value & 0x40 ? 1 : 0)) & 0xff;
            tmpAddress = gbDataMMM01.mapperRomBank0Remapping << 18;
            tmpAddress &= gbRomSizeMask;
            MBCswitchROMBank0(tmpAddress);
        }
        break;
    case 0x6000: // memory model select
        gbDataMMM01.mapperMemoryModel = value & 1;
        break;
    }
}

// MMM01 RAM write
void mapperMMM01RAM(uint16_t address, uint8_t value)
{
    if (gbDataMMM01.mapperRAMEnable) {
        if (gbRamSize) {
            gbMemoryMap[address >> 12][address & 0x0fff] = value;
            systemSaveUpdateCounter = SYSTEM_SAVE_UPDATED;
        }
    }
}

void memoryUpdateMapMMM01()
{
    int tmpAddress = gbDataMMM01.mapperROMBank << 14;

    // check current model
    if (gbDataMMM01.mapperMemoryModel == 1) {
        // model is 16/8, so we have a high address in use
        tmpAddress |= (gbDataMMM01.mapperROMHighAddress) << 19;
    }

    tmpAddress &= gbRomSizeMask;
    MBCswitchROMBank1(tmpAddress);

    tmpAddress = gbDataMMM01.mapperRomBank0Remapping << 18;
    tmpAddress &= gbRomSizeMask;
    MBCswitchROMBank0(tmpAddress);

    if (gbRamSize) {
        MBCswitchRAMBank(gbDataMMM01.mapperRAMAddress);
    }
}

// GameGenie ROM write registers
void mapperGGROM(uint16_t address, uint8_t value)
{
    switch (address & 0x6000) {
    case 0x0000: // RAM enable register
        break;
    case 0x2000: // GameGenie has only a half bank
        break;
    case 0x4000: // GameGenie has no RAM
        if ((address >= 0x4001) && (address <= 0x4020)) // GG Hardware Registers
            gbMemoryMap[address >> 12][address & 0x0fff] = value;
        break;
    case 0x6000: // GameGenie has only a half bank
        break;
    }
}

// GS3 Used to emulate the GS V3.0 rom bank switching
mapperGS3 gbDataGS3 = { 1 }; // ROM bank

void mapperGS3ROM(uint16_t address, uint8_t value)
{
    int tmpAddress = 0;

    switch (address & 0x6000) {
    case 0x0000: // GS has no ram
        break;
    case 0x2000: // GS has no 'classic' ROM bank select
        break;
    case 0x4000: // GS has no ram
        break;
    case 0x6000: // 0x6000 area is RW, and used for GS hardware registers

        if (address == 0x7FE1) // This is the (half) ROM bank select register
        {
            if (value == gbDataGS3.mapperROMBank)
                break;
            tmpAddress = value << 13;

            tmpAddress &= gbRomSizeMask;
            gbDataGS3.mapperROMBank = value;
            gbMemoryMap[GB_MEM_CART_BANK1] = &gbRom[tmpAddress];
            gbMemoryMap[GB_MEM_CART_BANK1 + 1] = &gbRom[tmpAddress + 0x1000];
        } else
            gbMemoryMap[address >> 12][address & 0x0fff] = value;
        break;
    }
}

void memoryUpdateMapGS3()
{
    int tmpAddress = gbDataGS3.mapperROMBank << 13;

    tmpAddress &= gbRomSizeMask;
    // GS can only change a half ROM bank
    gbMemoryMap[GB_MEM_CART_BANK1] = &gbRom[tmpAddress];
    gbMemoryMap[GB_MEM_CART_BANK1 + 1] = &gbRom[tmpAddress + 0x1000];
}

#define HEAD_ROMTYPE 0x147
#define HEAD_ROMSIZE 0x148
#define HEAD_RAMSIZE 0x149

bool gbUpdateSizes()
{
   if (gbRom[HEAD_ROMSIZE] > 8)
   {
      systemMessage(MSG_UNSUPPORTED_ROM_SIZE,
          N_("Unsupported rom size %02x"), gbRom[HEAD_ROMSIZE]);
      return false;
   }

   if (gbRomSize < gbRomSizes[gbRom[HEAD_ROMSIZE]])
   {
      uint8_t* gbRomNew = (uint8_t*)realloc(gbRom, gbRomSizes[gbRom[HEAD_ROMSIZE]]);
      if (!gbRomNew)
      {
         return false;
      };
      gbRom = gbRomNew;
      for (int i = gbRomSize; i < gbRomSizes[gbRom[HEAD_ROMSIZE]]; i++)
         gbRom[i] = 0x00; // Not sure if it's 0x00, 0xff or random data...
   }
   // (it's in the case a cart is 'lying' on its size.
   else if ((gbRomSize > gbRomSizes[gbRom[HEAD_ROMSIZE]]) && (genericflashcardEnable))
   {
      gbRomSize = gbRomSize >> 16;
      gbRom[HEAD_ROMSIZE] = 0;
      if (gbRomSize)
      {
         while (!((gbRomSize & 1) || (gbRom[HEAD_ROMSIZE] == 7)))
         {
            gbRom[HEAD_ROMSIZE]++;
            gbRomSize >>= 1;
         }
         gbRom[HEAD_ROMSIZE]++;
      }
      uint8_t* gbRomNew = (uint8_t*)realloc(gbRom, gbRomSizes[gbRom[HEAD_ROMSIZE]]);
      if (!gbRomNew)
      {
         return false;
      };
      gbRom = gbRomNew;
   }
   gbRomSize = gbRomSizes[gbRom[HEAD_ROMSIZE]];
   gbRomSizeMask = gbRomSizesMasks[gbRom[HEAD_ROMSIZE]];

   // The 'genericflashcard' option allows some PD to work.
   // However, the setting is dangerous (if you let in enabled
   // and play a normal game, it might just break everything).
   // That's why it is not saved in the emulator options.
   // Also I added some checks in VBA to make sure your saves will not be
   // overwritten if you wrongly enable this option for a game
   // you already played (and vice-versa, ie. if you forgot to
   // enable the option for a game you played with it enabled, like Shawu Story).
   uint8_t ramsize = genericflashcardEnable ? 5 : gbRom[HEAD_RAMSIZE];
   gbRom[HEAD_RAMSIZE] = ramsize;
   gbCheatingDevice = 0;
   if ((gbRom[2] == 0x6D) && (gbRom[5] == 0x47) && (gbRom[6] == 0x65) && (gbRom[7] == 0x6E) && (gbRom[8] == 0x69) && (gbRom[9] == 0x65) && (gbRom[0xA] == 0x28) && (gbRom[0xB] == 0x54))
   {
      gbCheatingDevice = 1; // GameGenie
      for (int i = 0; i < 0x20; i++) // Cleans GG hardware registers
         gbRom[0x4000 + i] = 0;
   }
   else if (((gbRom[0x104] == 0x44) && (gbRom[0x156] == 0xEA) && (gbRom[0x158] == 0x7F) && (gbRom[0x159] == 0xEA) && (gbRom[0x15B] == 0x7F)) || ((gbRom[0x165] == 0x3E) && (gbRom[0x166] == 0xD9) && (gbRom[0x16D] == 0xE1) && (gbRom[0x16E] == 0x7F)))
      gbCheatingDevice = 2; // GameShark      

   if (ramsize > 5)
   {
      systemMessage(MSG_UNSUPPORTED_RAM_SIZE,
          N_("Unsupported ram size %02x"), gbRom[HEAD_RAMSIZE]);
      return false;
   }

   gbRamSize = gbRamSizes[ramsize];
   gbRamSizeMask = gbRamSizesMasks[ramsize];

   gbRomType = gbRom[HEAD_ROMTYPE];
   if (genericflashcardEnable)
   {
      /*if (gbRomType<2)
      gbRomType =3;
    else if ((gbRomType == 0xc) || (gbRomType == 0xf) || (gbRomType == 0x12) ||
             (gbRomType == 0x16) || (gbRomType == 0x1a) || (gbRomType == 0x1d))
      gbRomType++;
    else if ((gbRomType == 0xb) || (gbRomType == 0x11) || (gbRomType == 0x15) ||
             (gbRomType == 0x19) || (gbRomType == 0x1c))
      gbRomType+=2;
    else if ((gbRomType == 0x5) || (gbRomType == 0x6))
      gbRomType = 0x1a;*/
      gbRomType = 0x1b;
   }
   else if (gbCheatingDevice == 1)
      gbRomType = 0x55;
   else if (gbCheatingDevice == 2)
      gbRomType = 0x56;

   if (genericflashcardEnable || gbCheatingDevice)
      gbRom[HEAD_ROMTYPE] = gbRomType;

   mapperRam = false;
   gbRTCPresent = false;
   gbRumble = false;
   gbBattery = false;

   switch (gbRomType)
   {
   case 0x00:
      mapperType = MBC1;
      mapperRam = true;
      break;

   case 0x01:
      mapperType = MBC1;
      mapperRam = true;
      break;

   case 0x02:
      mapperType = MBC1;
      mapperRam = true;
      break;

   case 0x03:
      mapperType = MBC1;
      mapperRam = true;
      gbBattery = true;
      break;

   case 0x05:
      mapperType = MBC2;
      gbRamSize = 0x200;
      gbRamSizeMask = 0x1ff;
      break;

   case 0x06:
      mapperType = MBC2;
      gbBattery = true;
      gbRamSize = 0x200;
      gbRamSizeMask = 0x1ff;
      break;

   case 0x08:
      mapperType = MBC1;
      mapperRam = true;
      break;

   case 0x09:
      mapperType = MBC1;
      mapperRam = true;
      gbBattery = true;
      break;

   case 0x0b:
      mapperType = MMM01;
      //mapperRam = true;
      break;
      
   case 0x0c:
      mapperType = MMM01; 
      break;

   case 0x0d:
      mapperType = MMM01;
      gbBattery = true;
      break;

   case 0x0f:
      mapperType = MBC3;
      mapperRam = true;
      gbBattery = true;
      gbRTCPresent = true;
      break;

   case 0x10:
      mapperType = MBC3;
      mapperRam = true;
      gbBattery = true;
      gbRTCPresent = true;
      break;

   case 0x11:
      mapperType = MBC3;
      mapperRam = true;
      break;

   case 0x12:
      mapperType = MBC3;
      mapperRam = true;
      break;

   case 0x13:
      mapperType = MBC3;
      mapperRam = true;
      gbBattery = true;
      break;

   case 0x19:
      mapperType = MBC5;
      mapperRam = true;
      break;

   case 0x1a:
      mapperType = MBC5;
      mapperRam = true;
      break;

   case 0x1b:
      mapperType = MBC5;
      mapperRam = true;
      break;

   case 0x1c:
      mapperType = MBC5;
      mapperRam = true;
      gbRumble = true;
      break;

   case 0x1d:
      mapperType = MBC5;
      mapperRam = true;
      gbRumble = true;
      break;

   case 0x1e:
      mapperType = MBC5;
      mapperRam = true;
      gbRumble = true;
      break;

   case 0x22:
      mapperType = MBC7;
      mapperRam = true;
      gbBattery = true;
      gbRamSize = 0x200;
      gbRamSizeMask = 0x1ff;
      break;

   // GG (GameGenie)
   case 0x55:
      mapperType = GAMEGENIE;
      break;

   case 0x56:
      // GS (GameShark)
      mapperType = GAMESHARK;
      break;

   case 0xfc:
      mapperType = MBC3;
      mapperRam = true;
      break;

   case 0xfd:
      // TAMA5
      if (gbRam != NULL)
      {
         free(gbRam);
         gbRam = NULL;
      }

      ramsize = 3;
      gbRamSize = gbRamSizes[3];
      gbRamSizeMask = gbRamSizesMasks[3];
      gbRamFill = 0x0;

      gbTAMA5ramSize = 0x100;

      if (gbTAMA5ram == NULL) {
         gbTAMA5ram = (uint8_t*)malloc(gbTAMA5ramSize);
         if (!gbTAMA5ram) return false;
      }
      memset(gbTAMA5ram, 0x0, gbTAMA5ramSize);

      mapperType = TAMA5;
      mapperRam = true;
      gbBattery = true;
      gbRTCPresent = true;
      break;

   case 0xfe:
      mapperType = HUC3;
      mapperRam = true;
      break;

   case 0xff:
      mapperType = HUC1;
      gbBattery = true;
      break;

   default:
      systemMessage(MSG_UNKNOWN_CARTRIDGE_TYPE,
          N_("Unknown cartridge type %02x"), gbRomType);
      return false;
   }

   if (gbRamSize)
   {
      gbRam = (uint8_t*)malloc(gbRamSize);
      if (!gbRam) return false;
      memset(gbRam, gbRamFill, gbRamSize);
   }

   if (!gbInit()) return false;

   return true;
}

void mapperWrite(uint16_t address, uint8_t value)
{
    if (address >= 0xA000 && address < 0xC000)
    {
        switch (mapperType)
        {
        case MBC1:
            mapperMBC1RAM(address, value);
            return;
        case MBC2:
            mapperMBC2RAM(address, value);
            return;
        case MBC3:
            mapperMBC3RAM(address, value);
            return;
        case MBC5:
            mapperMBC5RAM(address, value);
            return;
        case MBC7:
            mapperMBC7RAM(address, value);
            return;
        case MMM01:
            mapperMMM01RAM(address, value);
            return;
        case GAMEGENIE:
            return;
        case GAMESHARK:
            return;
        case TAMA5:
            mapperTAMA5RAM(address, value);
            return;
        case HUC1:
            mapperHuC1RAM(address, value);
            return;
        case HUC3:
            mapperHuC3RAM(address, value);
            return;
        default:
            return;
        }
    }

    switch (mapperType)
    {
    case MBC1:
        mapperMBC1ROM(address, value);
        return;
    case MBC2:
        mapperMBC2ROM(address, value);
        return;
    case MBC3:
        mapperMBC3ROM(address, value);
        return;
    case MBC5:
        mapperMBC5ROM(address, value);
        return;
    case MBC7:
        mapperMBC7ROM(address, value);
        return;
    case MMM01:
        mapperMMM01ROM(address, value);
        return;
    case GAMEGENIE:
        mapperGGROM(address, value);
        return;
    case GAMESHARK:
        mapperGS3ROM(address, value);
        return;
    case TAMA5:
        return;
    case HUC1:
        mapperHuC1ROM(address, value);
        return;
    case HUC3:
        mapperHuC3ROM(address, value);
        return;
    default:
        return;
    }
}

uint8_t mapperReadRAM(uint16_t address)
{
    switch (mapperType)
    {
    case MBC1:
        return mapperMBC1ReadRAM(address);
    case MBC2:
        break;
    case MBC3:
        return mapperMBC3ReadRAM(address);
    case MBC5:
        return mapperMBC5ReadRAM(address);
    case MBC7:
        return mapperMBC7ReadRAM(address);
    case MMM01:
        break;
    case GAMEGENIE:
        break;
    case GAMESHARK:
        break;
    case TAMA5:
        return mapperTAMA5ReadRAM(address);
    case HUC1:
        break;
    case HUC3:
        return mapperHuC3ReadRAM(address);
    default:
        break;
    }

    return gbMemoryMap[address >> 12][address & 0x0fff];
}

void mapperUpdateMap()
{
    switch (mapperType)
    {
    case MBC1:
        memoryUpdateMapMBC1();
        return;
    case MBC2:
        memoryUpdateMapMBC2();
        return;
    case MBC3:
        memoryUpdateMapMBC3();
        return;
    case MBC5:
        memoryUpdateMapMBC5();
        return;
    case MBC7:
        memoryUpdateMapMBC7();
        return;
    case MMM01:
        memoryUpdateMapMMM01();
        return;
    case GAMEGENIE:
        return;
    case GAMESHARK:
        memoryUpdateMapGS3();
        return;
    case TAMA5:
        memoryUpdateMapTAMA5();
        return;
    case HUC1:
        memoryUpdateMapHuC1();
        return;
    case HUC3:
        memoryUpdateMapHuC3();
        return;
    default:
        return;
    }
}

void gbMbcSaveGame(uint8_t *&data)
{
   switch (mapperType)
   {
   case MBC1:
      utilWriteMem(data, &gbDataMBC1, sizeof(gbDataMBC1));
      break;
   case MBC2:
      utilWriteMem(data, &gbDataMBC2, sizeof(gbDataMBC2));
      break;
   case MBC3:
      utilWriteMem(data, &gbDataMBC3, sizeof(gbDataMBC3));
      break;
   case MBC5:
      utilWriteMem(data, &gbDataMBC5, sizeof(gbDataMBC5));
      break;
   case MBC7:
      break;
   case MMM01:
      utilWriteMem(data, &gbDataMMM01, sizeof(gbDataMMM01));
      break;
   case GAMEGENIE:
      break;
   case GAMESHARK:
      break;
   case TAMA5:
      utilWriteMem(data, &gbDataTAMA5, sizeof(gbDataTAMA5));
      if (gbTAMA5ram != NULL)
         utilWriteMem(data, gbTAMA5ram, gbTAMA5ramSize);
      break;
   case HUC1:
      utilWriteMem(data, &gbDataHuC1, sizeof(gbDataHuC1));
      break;
   case HUC3:
      utilWriteMem(data, &gbDataHuC3, sizeof(gbDataHuC3));
      break;
   default:
      break;
   }

   if (gbRTCPresent)
      utilWriteMem(data, &rtcData, sizeof(rtcData));
}

void gbMbcReadGame(const uint8_t *&data, int version)
{
   switch (mapperType)
   {
   case MBC1:
      utilReadMem(&gbDataMBC1, data, sizeof(gbDataMBC1));
      break;
   case MBC2:
      utilReadMem(&gbDataMBC2, data, sizeof(gbDataMBC2));
      break;
   case MBC3:
      utilReadMem(&gbDataMBC3, data, sizeof(gbDataMBC3));
      break;
   case MBC5:
      utilReadMem(&gbDataMBC5, data, sizeof(gbDataMBC5));
      break;
   case MBC7:
      break;
   case MMM01:
      utilReadMem(&gbDataMMM01, data, sizeof(gbDataMMM01));
      break;
   case GAMEGENIE:
      break;
   case GAMESHARK:
      break;
   case TAMA5:
      utilReadMem(&gbDataTAMA5, data, sizeof(gbDataTAMA5));
      if (gbTAMA5ram != NULL)
         utilReadMem(gbTAMA5ram, data, gbTAMA5ramSize);
      break;
   case HUC1:
      utilReadMem(&gbDataHuC1, data, sizeof(gbDataHuC1));
      break;
   case HUC3:
      utilReadMem(&gbDataHuC3, data, sizeof(gbDataHuC3));
      break;
   default:
      break;
   }

   if (gbRTCPresent)
      utilReadMem(&rtcData, data, sizeof(rtcData));
}