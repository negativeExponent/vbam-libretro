#ifndef GBACPU_H
#define GBACPU_H

extern int armExecute();
extern int thumbExecute();

#if defined(__i386__) || defined(__x86_64__)
#define INSN_REGPARM __attribute__((regparm(1)))
#else
#define INSN_REGPARM /*nothing*/
#endif

#ifdef __GNUC__
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#endif

#define UPDATE_REG(address, value)                 \
    {                                              \
        WRITE16LE(((uint16_t*)&ioMem[address]), value); \
    }

#define ARM_PREFETCH                                        \
    {                                                       \
        cpuPrefetch[0] = CPUReadMemoryQuick(armNextPC);     \
        cpuPrefetch[1] = CPUReadMemoryQuick(armNextPC + 4); \
    }

#define THUMB_PREFETCH                                        \
    {                                                         \
        cpuPrefetch[0] = CPUReadHalfWordQuick(armNextPC);     \
        cpuPrefetch[1] = CPUReadHalfWordQuick(armNextPC + 2); \
    }

#define ARM_PREFETCH_NEXT cpuPrefetch[1] = CPUReadMemoryQuick(armNextPC + 4);

#define THUMB_PREFETCH_NEXT cpuPrefetch[1] = CPUReadHalfWordQuick(armNextPC + 2);

extern int SWITicks;
extern uint32_t mastercode;
extern bool busPrefetch;
extern bool busPrefetchEnable;
extern uint32_t busPrefetchCount;
extern int cpuNextEvent;
extern bool holdState;
extern uint32_t cpuPrefetch[2];
extern int cpuTotalTicks;
extern uint8_t memoryWait[16];
extern uint8_t memoryWait32[16];
extern uint8_t memoryWaitSeq[16];
extern uint8_t memoryWaitSeq32[16];
extern uint8_t cpuBitsSet[256];
extern uint8_t cpuLowestBitSet[256];
extern void CPUUpdateCPSR();
extern void CPUSwitchMode(int mode, bool saveState, bool breakLoop);
extern void CPUUpdateFlags(bool breakLoop);
extern void CPUSoftwareInterrupt(int comment);

enum {
    BITS_16 = 0,
    BITS_32 = 1
};

#define DATATICKS_ACCESS_16(addr)     memoryWait[((addr) >> 24) & 15]
#define DATATICKS_ACCESS_32(addr)     memoryWait32[((addr) >> 24) & 15]
#define DATATICKS_ACCESS_16_SEQ(addr) memoryWaitSeq[((addr) >> 24) & 15]
#define DATATICKS_ACCESS_32_SEQ(addr) memoryWaitSeq32[((addr) >> 24) & 15]

// Waitstates when accessing data
inline void updateBusPrefetchCount(uint32_t address, int value)
{
    int addr = (address >> 24) & 15;
    switch (addr) {
    case 0x00:
    case 0x01:
    case 0x08:
    case 0x09:
    case 0x0A:
    case 0x0B:
    case 0x0C:
    case 0x0D:
    case 0x0E:
    case 0x0F:
        busPrefetchCount = 0;
        busPrefetch = false;
        return;
    default:
        if (busPrefetch) {
            int waitState = value;
            waitState = (1 & ~waitState) | (waitState & waitState);
            busPrefetchCount = ((busPrefetchCount + 1) << waitState) - 1;
        }
    }
}

// Waitstates when executing opcode
inline int codeTicksAccess(int bits32, uint32_t address) // ARM/THUMB NON SEQ
{
    int addr = (address >> 24) & 15;
    switch (addr) {
    case 0x08:
    case 0x09:
    case 0x0A:
    case 0x0B:
    case 0x0C:
    case 0x0D:
        if (busPrefetchCount & 0x1) {
            if (busPrefetchCount & 0x2) {
                busPrefetchCount = ((busPrefetchCount & 0xFF) >> 2) | (busPrefetchCount & 0xFFFFFF00);
                return 0;
            }
            busPrefetchCount = ((busPrefetchCount & 0xFF) >> 1) | (busPrefetchCount & 0xFFFFFF00);
            return memoryWaitSeq[addr] - 1;
        }
    }

    busPrefetchCount = 0;
    return bits32 ? memoryWait32[addr] : memoryWait[addr];
}

inline int codeTicksAccessSeq16(uint32_t address) // THUMB SEQ
{
    int addr = (address >> 24) & 15;
    switch (addr) {
    case 0x08:
    case 0x09:
    case 0x0A:
    case 0x0B:
    case 0x0C:
    case 0x0D:
        if (busPrefetchCount & 0x1) {
            busPrefetchCount = ((busPrefetchCount & 0xFF) >> 1) | (busPrefetchCount & 0xFFFFFF00);
            return 0;
        }
        if (busPrefetchCount > 0xFF) {
            busPrefetchCount = 0;
            return memoryWait[addr];
        }
        break;
    default:
        busPrefetchCount = 0;
    }

    return memoryWaitSeq[addr];
}

inline int codeTicksAccessSeq32(uint32_t address) // ARM SEQ
{
    int addr = (address >> 24) & 15;
    switch (addr) {
    case 0x08:
    case 0x09:
    case 0x0A:
    case 0x0B:
    case 0x0C:
    case 0x0D:
        if (busPrefetchCount & 0x1) {
            if (busPrefetchCount & 0x2) {
                busPrefetchCount = ((busPrefetchCount & 0xFF) >> 2) | (busPrefetchCount & 0xFFFFFF00);
                return 0;
            }
            busPrefetchCount = ((busPrefetchCount & 0xFF) >> 1) | (busPrefetchCount & 0xFFFFFF00);
            return memoryWaitSeq[addr];
        }
        if (busPrefetchCount > 0xFF) {
            busPrefetchCount = 0;
            return memoryWait32[addr];
        }
    }

    return memoryWaitSeq32[addr];
}

// Emulates the Cheat System (m) code
inline void cpuMasterCodeCheck()
{
    if ((mastercode) && (mastercode == armNextPC)) {
        uint32_t joy = 0;
        if (systemReadJoypads())
            joy = systemReadJoypad(-1);
        uint32_t ext = (joy >> 10);
        cpuTotalTicks += cheatsCheckKeys(P1 ^ 0x3FF, ext);
    }
}

#endif // GBACPU_H
