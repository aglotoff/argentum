#include <stdint.h>

#include "console.h"
#include "memlayout.h"
#include "sbcon.h"
#include "vm.h"

static const char *daynames[] = {
  [1]  "Sun",   [2]  "Mon",   [3]  "Tue",   [4]  "Wed",
  [5]  "Thu",   [6]  "Fri",   [7]  "Sat"
};

static const char *monthnames[] = {
  [1]  "Jan",   [2]  "Feb",   [3]  "Mar",   [4]  "Apr",
  [5]  "May",   [6]  "Jun",   [7]  "Jul",   [8]  "Aug",
  [9]  "Sep",   [10] "Oct",   [11] "Nov",   [12] "Dec"
};

static uint8_t sb_read(uint8_t addr, uint8_t reg);
void sb_write(uint8_t addr, uint8_t reg, uint8_t data);

static void sb_delay(void);
static void sb_start(void);
static void sb_stop(void);
static uint8_t sb_rx_byte(uint8_t data);
static uint8_t sb_tx_byte(uint8_t data);

static volatile uint32_t *sb;

void
sb_init(void)
{
  sb = (volatile uint32_t *) vm_map_mmio(SB_CON0, 4096);

  sb[SB_CONTROLS] = SCL;
  sb[SB_CONTROLS] = SDA;
}

void
sb_rtc_time(void)
{
  int sec, min, hour, mday, mon, year, wday;

  sec = sb_read(SB_RTC, RTC_SECONDS);
  sec = ((sec >> 4) & 0x7) * 10 + (sec & 0xF);

  min = sb_read(SB_RTC, RTC_MINUTES);
  min = ((min >> 4) & 0x7) * 10 + (min & 0xF);

  hour = sb_read(SB_RTC, RTC_HOURS);
  if (hour & 0x40) {
    hour = ((hour >> 4) & 0x1) * 10 + (hour & 0xF) + (hour & 0x20 ? 12 : 0);
    if (hour % 12 == 0) {
      hour -= 12;
    }
  } else {
    hour = ((hour >> 4) & 0x3) * 10 + (hour & 0xF);
  }

  mday = sb_read(SB_RTC, RTC_DATE);
  mday = ((mday >> 4) & 0x3) * 10 + (mday & 0xF);

  mon = sb_read(SB_RTC, RTC_MONTH);
  mon = ((mon >> 4) & 0x1) * 10 + (mon & 0xF);

  year = sb_read(SB_RTC, RTC_YEAR);
  year = 2000 + ((year >> 4) & 0xF) * 10 + (year & 0xF);

  wday = sb_read(SB_RTC, RTC_DAY);
  wday = wday & 0x7;

  cprintf("%s %s %d %02d:%02d:%02d %d\n",
          daynames[wday], monthnames[mon], mday, hour, min, sec, year);
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

// Send start sequence
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

// Sen stop sequence
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
