
#ifndef _ARCH_I386_IDE_H
#define _ARCH_I386_IDE_H

#include <stdint.h>

struct Buf;

int  ide_init(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
void ide_request(struct Buf *buf);

#endif  // !_ARCH_I386_IDE_H
