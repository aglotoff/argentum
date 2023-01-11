#include <stdint.h>
#include <string.h>

#include <argentum/drivers/rtc.h>
#include <argentum/mm/memlayout.h>
#include <argentum/mm/vm.h>
#include <argentum/spinlock.h>

#include "ds1338.h"
#include "sbcon.h"

// PBX-A9 has two serial bus inerfaces (SBCon0 and SBCon1). SBCon0 provides
// access to the Maxim DS1338 RTC on the baseboard.

// RTC device address
#define RTC_ADDR      0xD0            

static struct SBCon sbcon0;
static struct DS1338 toy;

static struct SpinLock rtc_lock;

/**
 * Initialize the RTC driver.
 */
void
rtc_init(void)
{
  sbcon_init(&sbcon0, PA2KVA(PHYS_CON0));
  ds1338_init(&toy, &sbcon0, RTC_ADDR);
  spin_init(&rtc_lock, "rtc");
}

/**
 * Get the current UTC time.
 */
time_t
rtc_time(void)
{
  struct tm t1, t2;

  spin_lock(&rtc_lock);

  // Make sure RTC doesn't modify the date while we're reading it.
  do {
    ds1338_get_time(&toy, &t1);
    ds1338_get_time(&toy, &t2);
  } while (memcmp(&t1, &t2, sizeof(struct tm)) != 0);

  spin_unlock(&rtc_lock);

  return mktime(&t2);
}
