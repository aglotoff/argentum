#ifndef __KERNEL_DRIVERS_SD_H__
#define __KERNEL_DRIVERS_SD_H__

#ifndef __AG_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

struct Buf;

void sd_init(void);
void sd_intr(void);
void sd_request(struct Buf *);

#endif  // !__KERNEL_DRIVERS_SD_H__
