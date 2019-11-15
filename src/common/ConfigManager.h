#ifndef _CONFIGMANAGER_H
#define _CONFIGMANAGER_H

#include <stdio.h>

#define MAX_CHEATS 16384

extern bool cpuIsMultiBoot;
extern bool mirroringEnable;
extern bool speedup;
extern const char *loadDotCodeFile;
extern const char *saveDotCodeFile;
extern int cheatsEnabled;
extern int cpuSaveType;
extern int frameSkip;
extern int layerEnable;
extern int layerSettings;
extern int romSize;
extern int rtcEnabled;
extern int saveType;
extern int sensorX;
extern int sensorY;
extern int sizeX;
extern int sizeY;
extern int skipBios;
extern int useBios;
extern int winGbPrinterEnabled;

#endif // _CONFIGMANAGER_H
