#ifndef __KERNEL_SD_H__
#define __KERNEL_SD_H__

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

struct Buf;

int  storage_init(void);
void storage_request(struct Buf *);

#endif  // !__KERNEL_SD_H__
