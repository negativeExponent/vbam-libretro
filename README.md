This is a minimal fork of libretro-vbam which itself is a fork of VBA-M (VisualBoyAdvance-M) for libretro with some perfomance enhancements, accuracy and bug fixes.

## Some changes includes:
* Speed improvements
* Passes more test for cpu, dma and io
* GB Sound Effects (echo, surround, stereo separation)
* Fix for Pinball Deluxe
* Fix for missing sprites in GBC
* GBA: Fix audio stutters in some games when using bios file
* Better GB to GBC compatibility for some games
* Better libretro integration like update memory maps, input bitmasks etc...
* others

<img src="https://github.com/negativeExponent/images/blob/master/01memory.png" width="240" height="160" /> <img src="https://github.com/negativeExponent/images/blob/master/02io.png" width="240" height="160" /> <img src="https://github.com/negativeExponent/images/blob/master/03timing.png" width="240" height="160" />
<img src="https://github.com/negativeExponent/images/blob/master/04timercountup.png" width="240" height="160" /> <img src="https://github.com/negativeExponent/images/blob/master/05carrytest.png" width="240" height="160" /> <img src="https://github.com/negativeExponent/images/blob/master/06biosmathV2.png" width="240" height="160" />
<img src="https://github.com/negativeExponent/images/blob/master/07dma.png" width="240" height="160" />

## How to compile (only libretro port):
```
cd src/libretro
make
```

[Official VBAM README](https://github.com/negativeExponent/vbam-libretro/blob/minimal/README_VBAM.md)  
[Official VBAM Github Page](https://github.com/visualboyadvance-m/visualboyadvance-m)  
