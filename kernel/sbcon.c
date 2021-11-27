#include <stdint.h>
#include <string.h>

#include "console.h"
#include "memlayout.h"
#include "sbcon.h"
#include "spinlock.h"
#include "vm.h"

/*
 * ----------------------------------------------------------------------------
 * Serial bus I/O code
 * ----------------------------------------------------------------------------
 * 
 * PBX-A9 has two serial bus inerfaces (SBCon0 and SBCon1). SBCon0 provides
 * access to the Maxim DS1338 RTC on the baseboard.
 *
 * For more info on serial bus programming, see this tutorial:
 * https://www.robot-electronics.co.uk/i2c-tutorial
 *
 */

#define SB_CON0       0x10002000    // SBCon0 memory base address

#define SCL           (1U << 0)     // Clock line
#define SDA           (1U << 1)     // Data line

// Serial bus registers, divided by 4 for use as uint32_t[] indices
#define SB_CONTROL    (0x000 / 4)   // Read serial control bits
#define SB_CONTROLS   (0x000 / 4)   // Set serial control bits
#define SB_CONTROLC   (0x004 / 4)   // Clear serial control bits

static uint8_t sb_read(uint8_t addr, uint8_t reg);
void           sb_write(uint8_t addr, uint8_t reg, uint8_t data);

static void    sb_delay(void);
static void    sb_start(void);
static void    sb_stop(void);
static uint8_t sb_rx_byte(uint8_t data);
static uint8_t sb_tx_byte(uint8_t data);

static volatile uint32_t *sb;

static struct Spinlock rtc_lock;

/**
 * Initialize the serial bus driver.
 */
void
sb_init(void)
{
  sb = (volatile uint32_t *) vm_map_mmio(SB_CON0, PAGE_SIZE);

  sb[SB_CONTROLS] = SCL;
  sb[SB_CONTROLS] = SDA;

  spin_init(&rtc_lock, "rtc");
}

// Read from a slave device
static uint8_t
sb_read(uint8_t addr, uint8_t reg)
{
  uint8_t data;

  sb_start();               // Send a start sequence
  sb_tx_byte(addr);         // Send the device write address (R/W bit low)
  sb_tx_byte(reg);          // Send the internal register number
  sb_start();               // Send a start sequence again (repeated start)
  sb_tx_byte(addr | 0x1);   // Send the device read address (R/W bit high)
  data = sb_rx_byte(1);     // Read data byte
  sb_stop();                // Send the stop sequence

  return data;
}

// Dummy delay routine
static void
sb_delay(void)
{
  // TODO: do something
}

// Send the start sequence
static void
sb_start(void)
{
  sb[SB_CONTROLS] = SDA;
  sb_delay();
  sb[SB_CONTROLS] = SCL;
  sb_delay();
  sb[SB_CONTROLC] = SDA;
  sb_delay();
  sb[SB_CONTROLC] = SCL;
  sb_delay();
}

// Send the stop sequence
static void
sb_stop(void)
{
  sb[SB_CONTROLC] = SDA;
  sb_delay();
  sb[SB_CONTROLS] = SCL;
  sb_delay();
  sb[SB_CONTROLS] = SDA;
  sb_delay();
}

// Receive 8 bits of data
static uint8_t
sb_rx_byte(uint8_t ack)
{
  uint8_t x, data = 0;

  sb[SB_CONTROLS] = SDA;

  for (x = 0; x < 8; x++) {
    data <<= 1;

    do {
      sb[SB_CONTROLS] = SCL;
    } while (!(sb[SB_CONTROL] >> (SCL-1)) & 0x1);
    sb_delay();

    if ((sb[SB_CONTROL] >> (SDA-1)) & 0x1) {
      data |= 1;
    }
    sb[SB_CONTROLC] = SCL;
  }

  // Send (N)ACK bit
  if (ack) {
    sb[SB_CONTROLS] = SDA;
  } else {
    sb[SB_CONTROLC] = SDA;
  }
  sb[SB_CONTROLS] = SCL;
  sb_delay();

  sb[SB_CONTROLC] = SCL;
  sb[SB_CONTROLS] = SDA;

  return data;
}

// Transmit 8 bits of data
static uint8_t
sb_tx_byte(uint8_t data)
{
  uint8_t bit, x;

  for (x = 0; x < 8; x++) {
    if (data & 0x80) {
      sb[SB_CONTROLS] = SDA;
    } else {
      sb[SB_CONTROLC] = SDA;
    }

    sb[SB_CONTROLS] = SCL;
    sb_delay();
    sb[SB_CONTROLC] = SCL;

    data <<= 1;
  }

  // Possible ACK bit
  sb[SB_CONTROLS] = SDA;
  sb[SB_CONTROLS] = SCL;
  bit = (sb[SB_CONTROL] >> (SDA-1)) & 0x1;
  sb[SB_CONTROLC] = SCL;

  return bit;
}

/*
 * ----------------------------------------------------------------------------
 * Time-of-Year RTC chip.
 * ----------------------------------------------------------------------------
 * 
 * For more info, see the Maxim DS1338 RTC data sheet.
 *
 */

// RTC device address
#define SB_RTC        0xD0            

// RTC Registers
#define RTC_SECONDS   0x00
#define RTC_MINUTES   0x01
#define RTC_HOURS     0x02
#define RTC_DAY       0x03
#define RTC_DATE      0x04
#define RTC_MONTH     0x05
#define RTC_YEAR      0x06
#define RTC_CONTROL   0x07

static void sb_rtc_fill_date(struct tm *);

/**
 * Get the current UTC time.
 */
time_t
sb_rtc_time(void)
{
  struct tm t1, t2;

  spin_lock(&rtc_lock);

  // Make sure RTC doesn't modify date while we read it.
  do {
    sb_rtc_fill_date(&t1);
    sb_rtc_fill_date(&t2);
  } while (memcmp(&t1, &t2, sizeof(struct tm)) != 0);

  spin_unlock(&rtc_lock);

  return mktime(&t1);
}

static void
sb_rtc_fill_date(struct tm *tm)
{
  // Days to start of month
  static const int daysto[][12] = {
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 },  // non-leap
    { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 },  // leap
  };

  int sec, min, hour, mday, mon, year, wday, is_leap;

  sec = sb_read(SB_RTC, RTC_SECONDS);
  sec = ((sec >> 4) & 0x7) * 10 + (sec & 0xF);

  min = sb_read(SB_RTC, RTC_MINUTES);
  min = ((min >> 4) & 0x7) * 10 + (min & 0xF);

  hour = sb_read(SB_RTC, RTC_HOURS);
  if (hour & 0x40) {
    hour = ((hour >> 4) & 0x1) * 10 + (hour & 0xF) + (hour & 0x20 ? 12 : 0);
    hour = (hour % 12 == 0) ? hour - 12 : hour;
  } else {
    hour = ((hour >> 4) & 0x3) * 10 + (hour & 0xF);
  }

  mday = sb_read(SB_RTC, RTC_DATE);
  mday = ((mday >> 4) & 0x3) * 10 + (mday & 0xF);

  mon = sb_read(SB_RTC, RTC_MONTH);
  mon = ((mon >> 4) & 0x1) * 10 + (mon & 0xF);

  year = sb_read(SB_RTC, RTC_YEAR);
  year = ((year >> 4) & 0xF) * 10 + (year & 0xF);

  wday = sb_read(SB_RTC, RTC_DAY);
  wday = (wday & 0x7);

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
