#ifndef __KERNEL_DRIVERS_SD_H__
#define __KERNEL_DRIVERS_SD_H__

#include <kernel/core/list.h>
#include <kernel/spinlock.h>

#define SD_BLOCKLEN               512         // Single block length in bytes
#define SD_BLOCKLEN_LOG           9           // log2 of SD_BLOCKLEN

// Response types
#define SD_RESPONSE_R1            1
#define SD_RESPONSE_R1B           2
#define SD_RESPONSE_R2            3
#define SD_RESPONSE_R3            4
#define SD_RESPONSE_R6            7
#define SD_RESPONSE_R7            8

struct PL180;
struct Buf;

struct SD {
  struct KListLink queue;
  struct KSpinLock lock;
  struct PL180 *mmci;
};

int  sd_init(struct SD *, struct PL180 *, int);
void sd_request(struct SD *, struct Buf *);

#endif  // !__KERNEL_DRIVERS_SD_H__
