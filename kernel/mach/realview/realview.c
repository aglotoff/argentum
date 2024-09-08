#include <kernel/mach.h>
#include <kernel/mm/memlayout.h>
#include <kernel/trap.h>
#include <kernel/irq.h>
#include <kernel/spinlock.h>
#include <kernel/fs/buf.h>
#include <kernel/page.h>
#include <kernel/dev.h>
#include <kernel/drivers/console.h>

#include <kernel/drivers/ds1338.h>
#include <kernel/drivers/sbcon.h>
#include <kernel/drivers/gic.h>
#include <kernel/drivers/ptimer.h>
#include <kernel/drivers/sp804.h>
#include <kernel/drivers/pl180.h>
#include <kernel/drivers/sd.h>
#include <kernel/drivers/kbd.h>
#include <kernel/drivers/pl050.h>
#include <kernel/drivers/pl011.h>
#include <kernel/drivers/uart.h>
#include <kernel/drivers/pl111.h>
#include <kernel/drivers/display.h>
#include <kernel/drivers/lan9118.h>

// #define PHYS_GICC         0x1F000100    ///< Interrupt interface
#define PHYS_PTIMER       0x1F000600    ///< Private timer
// #define PHYS_GICD         0x1F001000    ///< Distributor

#define TICK_RATE     100U          // Desired timer events rate, in Hz

static struct Gic gic;
static struct PTimer ptimer;
static struct Sp804 timer01;

static void
realview_interrupt_ipi(void)
{
  gic_sgi(&gic, 0);
}

static int
realview_interrupt_id(void)
{
  return gic_intid(&gic);
}

static void
realview_interrupt_enable(int irq, int cpu)
{
  gic_setup(&gic, irq, cpu);
}

static void
realview_interrupt_mask(int irq)
{
  gic_disable(&gic, irq);
}

static void
realview_interrupt_unmask(int irq)
{
  gic_enable(&gic, irq);
}

static void
realview_interrupt_init_pb_a8(void)
{
  gic_init(&gic, PA2KVA(0x1E000000), PA2KVA(0x1E001000));
}

static void
realview_interrupt_init_pbx_a9(void)
{
  gic_init(&gic, PA2KVA(0x1F000100), PA2KVA(0x1F001000));

  k_irq_attach(0, ipi_irq, NULL);

  *(volatile int *) PA2KVA(0x10000030) = 0x10000;
  gic_sgi(&gic, 0);
}

static void
realview_interrupt_init_percpu(void)
{
  gic_init_percpu(&gic);
  interrupt_unmask(0);
}

static void
realview_interrupt_eoi(int irq)
{
  gic_eoi(&gic, irq);
}

/*******************************************************************************
 * Time-of-Year RTC driver.
 * 
 * PBX-A9 has two serial bus inerfaces (SBCon0 and SBCon1). SBCon0 provides
 * access to the Maxim DS1338 RTC on the baseboard.
 ******************************************************************************/

// RTC device address on the I2C bus
#define RTC_ADDR    0xD0            

static struct SBCon sbcon0;
static struct DS1338 rtc;

static struct KSpinLock  rtc_lock;

/**
 * Initialize the RTC driver.
 */
static void
realview_rtc_init(void)
{
  sbcon_init(&sbcon0, PA2KVA(PHYS_CON0));
  ds1338_init(&rtc, &sbcon0, RTC_ADDR);
  k_spinlock_init(&rtc_lock, "rtc");
}

/**
 * Get the current UTC time.
 * 
 * @return The current UTC time.
 */
static time_t
realview_rtc_get_time(void)
{
  struct tm tm;

  k_spinlock_acquire(&rtc_lock);
  ds1338_get_time(&rtc, &tm);
  k_spinlock_release(&rtc_lock);

  return mktime(&tm);
}

/**
 * Set the current UTC time.
 * 
 * @param time  New time value.
 */
static void
realview_rtc_set_time(time_t time)
{
  k_spinlock_acquire(&rtc_lock);
  ds1338_get_time(&rtc, gmtime(&time));
  k_spinlock_release(&rtc_lock);
}

static int
realview_pb_a8_timer_irq(void *arg)
{
  sp804_eoi(&timer01);
  return timer_irq(arg);
}

static void
realview_pb_a8_timer_init(void)
{
  sp804_init(&timer01, PA2KVA(0x10011000), TICK_RATE);
  k_irq_attach(36, realview_pb_a8_timer_irq, NULL);
}

static void
realview_pb_a8_timer_init_percpu(void)
{

}

struct PL180 mmci;
static struct SD sd;

void
realview_storage_request(struct Buf *buf)
{
  sd_request(&sd, buf);
}

struct BlockDev storage_dev = {
  .request = realview_storage_request,
};

int
realview_storage_init(void)
{
  pl180_init(&mmci, PA2KVA(PHYS_MMCI));
  sd_init(&sd, &mmci, IRQ_MCIA);
  dev_register_block(0, &storage_dev);
  return 0;
}

// PBX-A9 has two KMIs: KMI0 is used for the keyboard and KMI1 is used for the
// mouse.
static struct Pl050 kmi0;    

#define UART_CLOCK        24000000U     // UART clock rate, in Hz
#define UART_BAUD_RATE    115200        // Required baud rate

// Use UART0 as serial debug console.
static struct Uart uart0;
static struct Pl011 pl011;

static struct Display display;
static struct Pl111 lcd;

int
realview_console_init(void)
{
  struct Page *page;

  // Allocate the frame buffer.
  if ((page = page_alloc_block(8, PAGE_ALLOC_ZERO, PAGE_TAG_FB)) == NULL)
    panic("cannot allocate framebuffer");

  page->ref_count++;

  pl111_init(&lcd, PA2KVA(PHYS_LCD), page2pa(page), PL111_RES_VGA);

  display_init(&display, page2kva(page));

  pl050_init(&kmi0, PA2KVA(PHYS_KMI0), IRQ_KMI0);

  pl011_init(&pl011, PA2KVA(0x10009000), UART_CLOCK, UART_BAUD_RATE);
  uart_init(&uart0, &pl011, IRQ_UART0);

  return 0;
}

int
realview_console_getc(void)
{
  int c;

  if ((c = pl050_kbd_getc(&kmi0)) > 0)
    return c;
  
  return uart_getc(&uart0);
}

void
realview_console_putc(char c)
{
  uart_putc(&uart0, c);
  screen_out_char(tty_system->out.screen, c);
}

static struct Lan9118 lan9118;

int
realview_eth_init(void)
{
  lan9118_init(&lan9118);
  return 0;
}

void
realview_eth_write(const void *buf, size_t n)
{
  lan9118_write(&lan9118, buf, n);
}

#define NSCREENS    6   // For now, all ttys are screens

static struct Screen screens[NSCREENS];

static void
realview_tty_out_char(struct Tty *tty, char c)
{
  // The first (aka system) console is also connected to the serial port
  if (tty == tty_system)
    uart_putc(&uart0, c);

  screen_out_char(tty->out.screen, c);
}

static void
realview_tty_flush(struct Tty *tty)
{
  screen_flush(tty->out.screen);
}

static void
realview_tty_erase(struct Tty *tty)
{
  screen_backspace(tty->out.screen);
  realview_tty_out_char(tty, '\b');
  realview_tty_flush(tty);
}

static void
realview_tty_switch(struct Tty *tty)
{
  display_update(&display, tty->out.screen);
}

static void
realview_tty_init_system(void)
{
  mach_current->console_init();
}

static void
realview_tty_init(struct Tty *tty, int i)
{
  tty->out.screen = &screens[i];
  screen_init(tty->out.screen, &display);
}

MACH_DEFINE(realview_pb_a8) {
  .type = MACH_REALVIEW_PB_A8,

  .interrupt_ipi         = realview_interrupt_ipi,
  .interrupt_id          = realview_interrupt_id,
  .interrupt_enable      = realview_interrupt_enable,
  .interrupt_init        = realview_interrupt_init_pb_a8,
  .interrupt_init_percpu = realview_interrupt_init_percpu,
  .interrupt_mask        = realview_interrupt_mask,
  .interrupt_unmask      = realview_interrupt_unmask,
  .interrupt_eoi         = realview_interrupt_eoi,

  .timer_init            = realview_pb_a8_timer_init,
  .timer_init_percpu     = realview_pb_a8_timer_init_percpu,

  .rtc_init              = realview_rtc_init,
  .rtc_get_time          = realview_rtc_get_time,
  .rtc_set_time          = realview_rtc_set_time,

  .storage_init          = realview_storage_init,

  .console_init          = realview_console_init,
  .console_getc          = realview_console_getc,
  .console_putc          = realview_console_putc,

  .tty_erase             = realview_tty_erase,
  .tty_flush             = realview_tty_flush,
  .tty_init              = realview_tty_init,
  .tty_init_system       = realview_tty_init_system,
  .tty_out_char          = realview_tty_out_char,
  .tty_switch            = realview_tty_switch,

  .eth_init              = realview_eth_init,
  .eth_write             = realview_eth_write,
};

static int
realview_pbx_a9_timer_irq(void *arg)
{
  ptimer_eoi(&ptimer);
  return timer_irq(arg);
}

static void
realview_pbx_a9_timer_init(void)
{
  ptimer_init(&ptimer, PA2KVA(PHYS_PTIMER));
  ptimer_init_percpu(&ptimer, TICK_RATE);
  k_irq_attach(29, realview_pbx_a9_timer_irq, NULL);
}

static void
realview_pbx_a9_timer_init_percpu(void)
{
  ptimer_init_percpu(&ptimer, TICK_RATE);
  interrupt_unmask(29);
}

MACH_DEFINE(realview_pbx_a9) {
  .type = MACH_REALVIEW_PBX_A9,

  .interrupt_ipi         = realview_interrupt_ipi,
  .interrupt_id          = realview_interrupt_id,
  .interrupt_enable      = realview_interrupt_enable,
  .interrupt_init        = realview_interrupt_init_pbx_a9,
  .interrupt_init_percpu = realview_interrupt_init_percpu,
  .interrupt_mask        = realview_interrupt_mask,
  .interrupt_unmask      = realview_interrupt_unmask,
  .interrupt_eoi         = realview_interrupt_eoi,

  .timer_init            = realview_pbx_a9_timer_init,
  .timer_init_percpu     = realview_pbx_a9_timer_init_percpu,

  .rtc_init              = realview_rtc_init,
  .rtc_get_time          = realview_rtc_get_time,
  .rtc_set_time          = realview_rtc_set_time,

  .storage_init          = realview_storage_init,

  .console_init          = realview_console_init,
  .console_getc          = realview_console_getc,
  .console_putc          = realview_console_putc,

  .tty_erase             = realview_tty_erase,
  .tty_flush             = realview_tty_flush,
  .tty_init              = realview_tty_init,
  .tty_init_system       = realview_tty_init_system,
  .tty_out_char          = realview_tty_out_char,
  .tty_switch            = realview_tty_switch,

  .eth_init              = realview_eth_init,
  .eth_write             = realview_eth_write,
};
