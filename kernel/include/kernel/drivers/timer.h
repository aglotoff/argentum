#ifndef __KERNEL_DRIVERS_TIMER_H__
#define __KERNEL_DRIVERS_TIMER_H__

#ifndef __ARGENTUM_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

void timer_init(void);
void timer_intr(void);

#endif  // !__KERNEL_DRIVERS_TIMER_H__
