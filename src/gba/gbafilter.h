#ifndef __GBA_FILTER_H
#define __GBA_FILTER_H

#include "../System.h"

void gbafilter_pal(uint16_t * buf, int count);
void gbafilter_pal32(uint32_t * buf, int count);
void gbafilter_pad(uint8_t * buf, int count);

#endif // __GBA_FILTER_H