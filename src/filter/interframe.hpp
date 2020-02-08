/// Interframe blending filters

#ifndef INTERFRAME_HPP
#define INTERFRAME_HPP

extern int RGB_LOW_BITS_MASK;

static void Init(void);

// call ifc to ignore previous frame / when starting new
void InterframeCleanup(void);

extern void SmartIB(pixFormat *srcPtr, int width, int height);
extern void MotionBlurIB(pixFormat *srcPtr, int width, int height);

#endif // INTERFRAME_HPP