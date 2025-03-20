#include <time.h>

#include <kernel/time.h>
#include <kernel/console.h>
#include <kernel/core/spinlock.h>

#include <arch/i386/io.h>

enum {
  CMOS_ADDRESS = 0x70,
  CMOS_DATA    = 0x71,
};

enum {
  CMOS_ADDRESS_SECONDS  = 0x00,
  CMOS_ADDRESS_MINUTES  = 0x02,
  CMOS_ADDRESS_HOURS    = 0x04,
  CMOS_ADDRESS_WDAY     = 0x06,
  CMOS_ADDRESS_MDAY     = 0x07,
  CMOS_ADDRESS_MONTH    = 0x08,
  CMOS_ADDRESS_YEAR     = 0x09,
  CMOS_ADDRESS_STATUS_A = 0x0A,
  CMOS_ADDRESS_STATUS_B = 0x0B,
};

enum {
  CMOS_UIP = (1 << 7),
  CMOS_DM  = (1 << 2),
};

static const uint8_t CMOS_NMI_DISABLE = (1 << 7);

static uint8_t
bcd2bin(uint8_t value)
{
  return ((value >> 4) & 0xF) * 10 + (value & 0xF);
}

static void
cmos_select(uint8_t address)
{
  outb(CMOS_ADDRESS, CMOS_NMI_DISABLE | address);
}

static uint8_t
cmos_read(uint8_t address, int bcd)
{
  uint8_t value;
  
  cmos_select(address);
  value = inb(CMOS_DATA);

  return bcd ? bcd2bin(value) : value;
}

static void
cmos_get_time(struct tm *tm)
{
  // Days to start of month
  static const int daysto[][12] = {
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 },  // non-leap
    { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 },  // leap
  };

  int sec, min, hour, mday, mon, year, wday, is_leap;
  int bcd;

  bcd = !(cmos_read(CMOS_ADDRESS_STATUS_B, 0) & CMOS_DM);

  sec  = cmos_read(CMOS_ADDRESS_SECONDS, bcd);
  min  = cmos_read(CMOS_ADDRESS_MINUTES, bcd);
  hour = cmos_read(CMOS_ADDRESS_HOURS, bcd);
  mday = cmos_read(CMOS_ADDRESS_MDAY, bcd);
  mon  = cmos_read(CMOS_ADDRESS_MONTH, bcd);
  year = cmos_read(CMOS_ADDRESS_YEAR, bcd);
  wday = cmos_read(CMOS_ADDRESS_WDAY, bcd);

  is_leap = ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);

  tm->tm_sec   = sec;
  tm->tm_min   = min;
  tm->tm_hour  = hour;
  tm->tm_mday  = mday;
  tm->tm_mon   = mon - 1;
  tm->tm_year  = year + (2000 - 1900);
  tm->tm_yday  = daysto[is_leap][mon - 1] + mday - 1;
  tm->tm_wday  = wday - 1;
  tm->tm_isdst = 0;
}

static struct KSpinLock cmos_lock = K_SPINLOCK_INITIALIZER("cmos");

void
arch_time_init(void)
{
  // Do nothing
}

time_t
arch_get_time_seconds(void)
{
  struct tm tm;

  k_spinlock_acquire(&cmos_lock);
  cmos_get_time(&tm);
  k_spinlock_release(&cmos_lock);

  return mktime(&tm);
}
