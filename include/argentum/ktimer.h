#ifndef __INCLUDE_ARGENTUM_KTIMER_H__
#define __INCLUDE_ARGENTUM_KTIMER_H__

unsigned long long ktimer_get_tick(void);
void               ktimer_set_tick(unsigned long long);
void               ktimer_tick_isr(void);

#endif  // !__INCLUDE_ARGENTUM_KTIMER_H__
