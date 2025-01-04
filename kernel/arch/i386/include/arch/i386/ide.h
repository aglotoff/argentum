
#ifndef _ARCH_I386_IDE_H
#define _ARCH_I386_IDE_H

struct Buf;

int  ide_init(void);
void ide_request(struct Buf *buf);

#endif  // !_ARCH_I386_IDE_H
