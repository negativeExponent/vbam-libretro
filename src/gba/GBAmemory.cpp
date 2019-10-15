#include "GBAinline.h"

static inline uint32_t ROR(uint32_t value, uint32_t shift)
{
   if (!shift)
      return value;
   return ((value >> shift) | (value << (32 - shift)));
}

uint32_t CPUReadMemory(uint32_t address)
{
   uint32_t value = 0;
   switch (address >> 24)
   {
      case 0:
         if (address < 0x4000)
         {
            if (reg[15].I >> 24)
            {
               value = READ32LE((uint32_t*)&biosProtected);
            }
            else
            {
               value = READ32LE(bios + (address & 0x3FFC));
            }
         }
         else
         {
            goto unreadable;
         }
         break;
      case 2:
         value = READ32LE(workRAM + (address & 0x3FFFC));
         break;
      case 3:
         value = READ32LE(internalRAM + (address & 0x7FFC));
         break;
      case 4:
         if ((address < 0x4000400) && ioReadable[address & 0x3FC])
         {
            if (ioReadable[(address & 0x3FC) + 2])
            {
               value = READ32LE(ioMem + (address & 0x3FC));
               if ((address & 0x3FC) == COMM_JOY_RECV_L)
               {
                  UPDATE_REG(COMM_JOYSTAT, READ16LE(&ioMem[COMM_JOYSTAT]) & ~JOYSTAT_RECV);
               }
            }
            else
            {
               value = READ16LE(ioMem + (address & 0x3FC));
            }
         }
         else
         {
            goto unreadable;
         }
         break;
      case 5:
         value = READ32LE(paletteRAM + (address & 0x3FC));
         break;
      case 6:
      {
         uint32_t vram_addr = (address & 0x1FFFC);
         if (((DISPCNT & 7) > 2) && ((address & 0x1C000) == 0x18000))
         {
            return 0;
         }
         if ((address & 0x18000) == 0x18000)
         {
            vram_addr &= 0x17FFF;
         }
         value = READ32LE(vram + vram_addr);
         break;
      }
      case 7:
         value = READ32LE(oam + (address & 0x3FC));
         break;
      case 8:
      case 9:
      case 10:
      case 11:
      case 12:
         value = READ32LE(rom + (address & 0x1FFFFFC));
         break;
      case 13:
         if (cpuEEPROMEnabled)
         // no need to swap this
         {
            return eepromRead(address);
         }
         goto unreadable;
      case 14:
      case 15:
         if (cpuFlashEnabled | cpuSramEnabled)
         { // no need to swap this
            value = flashRead(address) * 0x01010101;
            break;
         }
      // default
      default:
      unreadable:
         if (cpuDmaHack)
         {
            value = cpuDmaLast;
         }
         else
         {
            value = CPUReadMemoryQuick(reg[15].I);
            if (!armState)
            {
               value &= 0xFFFF;
               value |= (value << 16);
            }
         }
   }
   int shift = (address & 3) << 3;
   return ROR(value, shift);
}

uint32_t CPUReadHalfWord(uint32_t address)
{
   uint32_t value = 0;
   switch (address >> 24)
   {
      case 0:
         if (address < 0x4000)
         {
            if (reg[15].I >> 24)
            {
               value = READ16LE(biosProtected + (address & 2));
            }
            else
            {
               value = READ16LE(bios + (address & 0x3FFE));
            }
         }
         else
         {
            goto unreadable;
         }
         break;
      case 2:
         value = READ16LE(workRAM + (address & 0x3FFFE));
         break;
      case 3:
         value = READ16LE(internalRAM + (address & 0x7FFE));
         break;
      case 4:
         if ((address < 0x4000400) && ioReadable[address & 0x3FE])
         {
            if ((address & 0xFF0) == 0x100)
            {
               switch (address & 0x10F)
               {
                  case 0x100:
                     if (timer0On)
                     {
                        value = 0xFFFF - ((timer0Ticks - cpuTotalTicks) >> timer0ClockReload);
                     }
                     break;
                  case 0x104:
                     if (timer1On && !(TM1CNT & 4))
                     {
                        value = 0xFFFF - ((timer1Ticks - cpuTotalTicks) >> timer1ClockReload);
                     }
                     break;
                  case 0x108:
                     if (timer2On && !(TM2CNT & 4))
                     {
                        value = 0xFFFF - ((timer2Ticks - cpuTotalTicks) >> timer2ClockReload);
                     }
                     break;
                  case 0x10C:
                     if (timer3On && !(TM3CNT & 4))
                     {
                        value = 0xFFFF - ((timer3Ticks - cpuTotalTicks) >> timer3ClockReload);
                     }
                     break;
               }
            }
            else
            {
               value = READ16LE(ioMem + (address & 0x3FE));
            }
         }
         else if ((address < 0x4000400) && ioReadable[address & 0x3FC])
         {
            return 0;
         }
         else
         {
            goto unreadable;
         }
         break;
      case 5:
         value = READ16LE(paletteRAM + (address & 0x3FE));
         break;
      case 6:
      {
         uint32_t vram_addr = (address & 0x1FFFE);
         if (((DISPCNT & 7) > 2) && ((address & 0x1C000) == 0x18000))
         {
            value = 0;
            break;
         }
         if ((address & 0x18000) == 0x18000)
         {
            vram_addr &= 0x17FFF;
         }
         value = READ16LE(vram + (vram_addr));
         break;
      }
      case 7:
         value = READ16LE(oam + (address & 0x3FE));
         break;
      case 8:
      case 9:
      case 10:
      case 11:
      case 12:
         if (address == 0x80000c4 || address == 0x80000c6 || address == 0x80000c8)
         {
            value = rtcRead(address);
         }
         else
         {
            value = READ16LE(rom + (address & 0x1FFFFFE));
         }
         break;
      case 13:
         if (cpuEEPROMEnabled)
         // no need to swap this
         {
            return eepromRead(address);
         }
         goto unreadable;
      case 14:
      case 15:
         if (cpuFlashEnabled | cpuSramEnabled)
         {
            // no need to swap this
            value = flashRead(address) * 0x0101;
            break;
         }
      // default
      default:
      unreadable:
         if (cpuDmaHack)
         {
            value = cpuDmaLast & 0xFFFF;
         }
         else
         {
            uint32_t param = reg[15].I;
            if (armState)
            {
               param += (address & 2);
            }
            value = CPUReadHalfWordQuick(param);
         }
   }
   int shift = (address & 1) << 3;
   return ROR(value, shift);
}

int16_t CPUReadHalfWordSigned(uint32_t address)
{
   int32_t value = (int32_t)CPUReadHalfWord(address);
   if ((address & 1))
   {
      return (int8_t)value;
   }
   return (int16_t)value;
}

uint8_t CPUReadByte(uint32_t address)
{
   switch (address >> 24)
   {
      case 0:
         if (address < 0x4000)
         {
            if (reg[15].I >> 24)
            {
               return biosProtected[address & 3];
            }
            else
            {
               return bios[address & 0x3FFF];
            }
         }
         else
         {
            goto unreadable;
         }
      case 2:
         return workRAM[address & 0x3FFFF];
      case 3:
         return internalRAM[address & 0x7FFF];
      case 4:
         if ((address < 0x4000400) && ioReadable[address & 0x3FF])
         {
            return ioMem[address & 0x3FF];
         }
         else
         {
            goto unreadable;
         }
      case 5:
         return paletteRAM[address & 0x3FF];
      case 6:
         address = (address & 0x1FFFF);
         if (((DISPCNT & 7) > 2) && ((address & 0x1C000) == 0x18000))
         {
            return 0;
         }
         if ((address & 0x18000) == 0x18000)
         {
            address &= 0x17FFF;
         }
         return vram[address];
      case 7:
         return oam[address & 0x3FF];
      case 8:
      case 9:
      case 10:
      case 11:
      case 12:
         return rom[address & 0x1FFFFFF];
      case 13:
         if (cpuEEPROMEnabled)
         {
            return eepromRead(address);
         }
         else
         {
            goto unreadable;
         }
      case 14:
      case 15:
         if (cpuSramEnabled | cpuFlashEnabled)
         {
            return flashRead(address);
         }

         switch (address & 0x00008f00)
         {
            case 0x8200:
               return systemGetSensorX() & 255;
            case 0x8300:
               return (systemGetSensorX() >> 8) | 0x80;
            case 0x8400:
               return systemGetSensorY() & 255;
            case 0x8500:
               return systemGetSensorY() >> 8;
         }
      // default
      default:
      unreadable:
         if (cpuDmaHack)
         {
            return cpuDmaLast & 0xFF;
         }
         else
         {
            uint32_t param = reg[15].I;
            if (armState)
            {
               param += (address & 3);
            }
            else
            {
               param += (address & 1);
            }
            return CPUReadByteQuick(param);
         }
   }
}

void CPUWriteMemory(uint32_t address, uint32_t value)
{
   switch (address >> 24)
   {
      case 0x02:
         WRITE32LE(workRAM + (address & 0x3FFFC), value);
         break;
      case 0x03:
         WRITE32LE(internalRAM + (address & 0x7FFC), value);
         break;
      case 0x04:
         if (address < 0x4000400)
         {
            CPUUpdateRegister((address & 0x3FC), (value & 0xFFFF));
            CPUUpdateRegister((address & 0x3FC) + 2, ((value >> 16) & 0xFFFF));
         }
         break;
      case 0x05:
         WRITE32LE(paletteRAM + (address & 0x3FC), value);
         break;
      case 0x06:
         address = (address & 0x1FFFC);
         if (((DISPCNT & 7) > 2) && ((address & 0x1C000) == 0x18000))
         {
            return;
         }
         if ((address & 0x18000) == 0x18000)
         {
            address &= 0x17FFF;
         }
         WRITE32LE(vram + address, value);
         break;
      case 0x07:
         WRITE32LE(oam + (address & 0x3FC), value);
         break;
      case 0x0D:
         if (cpuEEPROMEnabled)
         {
            eepromWrite(address, value);
         }
         break;
      case 0x0E:
      case 0x0F:
         if ((!eepromInUse) | cpuSramEnabled | cpuFlashEnabled)
         {
            (*cpuSaveGameFunc)(address, (uint8_t)value);
         }
         break;
   }
}

void CPUWriteHalfWord(uint32_t address, uint16_t value)
{
   switch (address >> 24)
   {
      case 2:
         WRITE16LE(workRAM + (address & 0x3FFFE), value);
         break;
      case 3:
         WRITE16LE(internalRAM + (address & 0x7FFE), value);
         break;
      case 4:
         if (address < 0x4000400)
         {
            CPUUpdateRegister(address & 0x3FE, value);
         }
         break;
      case 5:
         WRITE16LE(paletteRAM + (address & 0x3FE), value);
         break;
      case 6:
         address = (address & 0x1FFFE);
         if (((DISPCNT & 7) > 2) && ((address & 0x1C000) == 0x18000))
         {
            return;
         }
         if ((address & 0x18000) == 0x18000)
         {
            address &= 0x17FFF;
         }
         WRITE16LE(vram + address, value);
         break;
      case 7:
         WRITE16LE(oam + (address & 0x3FE), value);
         break;
      case 8:
      case 9:
         switch (address) {
            case 0x80000c4:
            case 0x80000c6:
            case 0x80000c8:
               (void)rtcWrite(address, value);
               break;
         }
         break;
      case 13:
         if (cpuEEPROMEnabled)
         {
            eepromWrite(address, (uint8_t)value);
         }
         break;
      case 14:
      case 15:
         if ((!eepromInUse) | cpuSramEnabled | cpuFlashEnabled)
         {
            (*cpuSaveGameFunc)(address, (uint8_t)value);
         }
         break;
   }
}

void CPUWriteByte(uint32_t address, uint8_t value)
{
   switch (address >> 24)
   {
      case 2:
         workRAM[address & 0x3FFFF] = value;
         break;
      case 3:
         internalRAM[address & 0x7FFF] = value;
         break;
      case 4:
         if (address < 0x4000400)
         {
            switch (address & 0x3FF)
            {
               case 0x60:
               case 0x61:
               case 0x62:
               case 0x63:
               case 0x64:
               case 0x65:
               case 0x68:
               case 0x69:
               case 0x6c:
               case 0x6d:
               case 0x70:
               case 0x71:
               case 0x72:
               case 0x73:
               case 0x74:
               case 0x75:
               case 0x78:
               case 0x79:
               case 0x7c:
               case 0x7d:
               case 0x80:
               case 0x81:
               case 0x84:
               case 0x85:
               case 0x90:
               case 0x91:
               case 0x92:
               case 0x93:
               case 0x94:
               case 0x95:
               case 0x96:
               case 0x97:
               case 0x98:
               case 0x99:
               case 0x9a:
               case 0x9b:
               case 0x9c:
               case 0x9d:
               case 0x9e:
               case 0x9f:
                  soundEvent8(address & 0xFF, value);
                  break;
               case 0x301: // HALTCNT, undocumented
                  if (value == 0x80)
                  {
                     stopState = true;
                  }
                  holdState = 1;
                  holdType = -1;
                  cpuNextEvent = cpuTotalTicks;
                  break;
               default: // every other register
                  uint32_t lowerBits = address & 0x3FE;
                  uint16_t tmp = READ16LE(ioMem + lowerBits);
                  if (address & 1)
                  {
                     tmp = (tmp & 0x00FF) | (value << 8);
                  }
                  else
                  {
                     tmp = (tmp & 0xFF00) | value;
                  }
                  CPUUpdateRegister(lowerBits, tmp);
                  break;
            }
            break;
         }
         break;
      case 5:
         // no need to switch
         WRITE16LE(paletteRAM + (address & 0x3FE), (value << 8) | value);
         break;
      case 6:
         address = (address & 0x1FFFE);
         if (((DISPCNT & 7) > 2) && ((address & 0x1C000) == 0x18000))
         {
            return;
         }
         if ((address & 0x18000) == 0x18000)
         {
            address &= 0x17FFF;
         }
         // no need to switch
         // byte writes to OBJ VRAM are ignored
         if ((address) < objTilesAddress[((DISPCNT & 7) + 1) >> 2])
         {
            WRITE16LE(vram + address, (value << 8) | value);
         }
         break;
      case 7:
         // no need to switch
         // byte writes to OAM are ignored
         //    *((uint16_t *)&oam[address & 0x3FE]) = (value << 8) | value;
         break;
      case 13:
         if (cpuEEPROMEnabled)
         {
            eepromWrite(address, value);
         }
         break;
      case 14:
      case 15:
         if ((saveType != 5) && ((!eepromInUse) | cpuSramEnabled | cpuFlashEnabled))
         {
            (*cpuSaveGameFunc)(address, value);
         }
         break;
   }
}
