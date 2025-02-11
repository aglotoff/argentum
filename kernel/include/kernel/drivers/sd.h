#ifndef __KERNEL_DRIVERS_SD_H__
#define __KERNEL_DRIVERS_SD_H__

#include <stdint.h>

#include <kernel/core/list.h>
#include <kernel/mutex.h>

#define SD_BLOCKLEN               512         // Single block length in bytes
#define SD_BLOCKLEN_LOG           9           // log2 of SD_BLOCKLEN

// Response types
#define SD_RESPONSE_R1            1
#define SD_RESPONSE_R1B           2
#define SD_RESPONSE_R2            3
#define SD_RESPONSE_R3            4
#define SD_RESPONSE_R6            7
#define SD_RESPONSE_R7            8

struct Buf;

struct SDOps {
  int  (*send_cmd)(void *, uint32_t, uint32_t, int, uint32_t *);
  int  (*irq_enable)(void *);
  int  (*begin_transfer)(void *, uint32_t, int);
  int  (*receive_data)(void *, void *, size_t);
  int  (*send_data)(void *, const void *, size_t);
};

struct SD {
  struct KListLink queue;
  struct KMutex    mutex;
  struct SDOps    *ops;
  void            *ctx;
};

int  sd_init(struct SD *, struct SDOps *, void *, int);
void sd_request(struct SD *, struct Buf *);

#endif  // !__KERNEL_DRIVERS_SD_H__
