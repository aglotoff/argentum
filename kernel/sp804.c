#include "sp804.h"

// Timer registers, shifted right by 2 bits for use as uint32_t[] 
#define TIMER1_CONTROL  (0x08 / 4)  // Control Register
#define TIMER1_INT_CLR  (0x0C / 4)  // Interrupt Clear Register
#define TIMER1_BG_LOAD  (0x18 / 4)  // Background Load Register

// Control Register bit assignments
#define TIMER_EN            (1 << 7)  // Enable
#define TIMER_MODE_PERIODIC (1 << 6)  // Mode: periodic
#define INT_ENABLE          (1 << 5)  // Interrupt Enable
#define TIMER_PRE_0         (0 << 2)  // 0 stages of prescale
#define TIMER_SIZE_32       (1 << 1)  // 32-bit counter

#define REF_CLOCK     1000000U      // 1MHz
#define TICK_RATE     100U          // Desired timer events rate, in Hz

int 
sp804_init(struct Sp804 *sp804, void *base)
{
  sp804->base = (volatile uint32_t *) base;

  sp804->base[TIMER1_BG_LOAD] = REF_CLOCK / TICK_RATE;
  sp804->base[TIMER1_CONTROL] = TIMER_SIZE_32 |
                                TIMER_MODE_PERIODIC |
                                INT_ENABLE |
                                TIMER_PRE_0 |
                                TIMER_EN;

  return 0;
}

void
sp804_eoi(struct Sp804 *sp804)
{
  // Writing random value clears the interrupt output
  sp804->base[TIMER1_INT_CLR] = 0xFFFFFFFF;
}
