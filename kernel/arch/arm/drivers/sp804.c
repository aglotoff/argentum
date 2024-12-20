// See the ARM Dual-Timer Module (SP804) Technical Reference Manual

#include <arch/arm/sp804.h>

// Timer registers
#define TIMER1_CONTROL      0x008     // Control Register
#define TIMER1_INT_CLR      0x00C     // Interrupt Clear Register
#define TIMER1_BG_LOAD      0x018     // Background Load Register
#define TIMER_PERIPH_ID0    0xFE0     // Timer Peripheral ID0 Register
#define TIMER_PERIPH_ID1    0xFE4     // Timer Peripheral ID1 Register
#define TIMER_PERIPH_ID2    0xFE8     // Timer Peripheral ID2 Register
#define TIMER_PERIPH_ID3    0xFEC     // Timer Peripheral ID3 Register
#define TIMER_PCELL_ID0     0xFF0     // Timer PrimeCell ID0 Register
#define TIMER_PCELL_ID1     0xFF4     // Timer PrimeCell ID1 Register
#define TIMER_PCELL_ID2     0xFF8     // Timer PrimeCell ID2 Register
#define TIMER_PCELL_ID3     0xFFC     // Timer PrimeCell ID3 Register

// Control Register bit assignments
#define TIMER_EN            (1 << 7)  // Enable
#define TIMER_MODE_PERIODIC (1 << 6)  // Mode: periodic
#define INT_ENABLE          (1 << 5)  // Interrupt Enable
#define TIMER_PRE_0         (0 << 2)  // 0 stages of prescale
#define TIMER_SIZE_32       (1 << 1)  // 32-bit counter

// Hard-coded values for identification registers
#define PERIPH_ID           0x00141804
#define PCELL_ID            0xB105F00D

#define REF_CLOCK           1000000U   // 1MHz

static inline uint32_t
sp804_read(struct Sp804 *sp804, uint32_t reg)
{
  return sp804->base[reg >> 2];
}

static inline void
sp804_write(struct Sp804 *sp804, uint32_t reg, uint32_t data)
{
  sp804->base[reg >> 2] = data;
}

int 
sp804_init(struct Sp804 *sp804, void *base, int rate)
{
  uint32_t periph_id, pcell_id;
  
  sp804->base = (volatile uint32_t *) base;

  periph_id = (sp804_read(sp804, TIMER_PERIPH_ID0) & 0xFF) |
             ((sp804_read(sp804, TIMER_PERIPH_ID1) & 0xFF) << 8) |
             ((sp804_read(sp804, TIMER_PERIPH_ID2) & 0xFF) << 16) |
             ((sp804_read(sp804, TIMER_PERIPH_ID3) & 0xFF) << 24);

  pcell_id = (sp804_read(sp804, TIMER_PCELL_ID0) & 0xFF) |
            ((sp804_read(sp804, TIMER_PCELL_ID1) & 0xFF) << 8) |
            ((sp804_read(sp804, TIMER_PCELL_ID2) & 0xFF) << 16) |
            ((sp804_read(sp804, TIMER_PCELL_ID3) & 0xFF) << 24);

  if ((periph_id != PERIPH_ID) || (pcell_id != PCELL_ID))
    return -1;

  sp804_write(sp804, TIMER1_BG_LOAD, REF_CLOCK / rate);
  sp804_write(sp804, TIMER1_CONTROL,
              TIMER_SIZE_32 |
              TIMER_MODE_PERIODIC |
              INT_ENABLE |
              TIMER_PRE_0 |
              TIMER_EN);

  return 0;
}

void
sp804_eoi(struct Sp804 *sp804)
{
  // Writing random value clears the interrupt output
  sp804_write(sp804, TIMER1_INT_CLR, 0xFFFFFFFF);
}
