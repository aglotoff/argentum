#ifndef __KERNEL_SD_H__
#define __KERNEL_SD_H__

struct Buf;

int  sd_init(void);
void sd_intr(void);
void sd_request(struct Buf *);

#endif  // !__KERNEL_SD_H__
