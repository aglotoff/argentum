#ifndef __KERNEL_DRIVERS_SD_H__
#define __KERNEL_DRIVERS_SD_H__

#ifndef __AG_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

struct Buf;

#define SD_BLOCKLEN               512         // Single block length in bytes
#define SD_BLOCKLEN_LOG           9           // log2 of SD_BLOCKLEN

// Response types
#define RESPONSE_R1               1
#define RESPONSE_R1B              2
#define RESPONSE_R2               3
#define RESPONSE_R3               4
#define RESPONSE_R6               7
#define RESPONSE_R7               8

void sd_init(void);
void sd_request(struct Buf *);

#endif  // !__KERNEL_DRIVERS_SD_H__
