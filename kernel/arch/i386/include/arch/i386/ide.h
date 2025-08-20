
#ifndef _ARCH_I386_IDE_H
#define _ARCH_I386_IDE_H

#include <stdint.h>

struct BufRequest;

int  ide_init(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
void ide_request(struct BufRequest *);

#endif  // !_ARCH_I386_IDE_H
