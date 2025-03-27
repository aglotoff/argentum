#include <kernel/console.h>
#include <kernel/interrupt.h>
#include <kernel/trap.h>
#include <kernel/core/cpu.h>
#include <kernel/core/irq.h>
#include <kernel/core/task.h>
#include <kernel/object_pool.h>
#include <kernel/page.h>

static int  interrupt_handler_call(int);
static void interrupt_task_entry(void *);
static int  interrupt_task_notify(int, void *);

// TODO: should be architecture-specific?
#define INTERRUPT_HANDLER_MAX       64

struct InterruptTask {
  struct KTask        task;
  void              (*handler)(int, void *);
  void               *handler_arg;
  int                 irq;
  struct KSemaphore   semaphore;
};

static struct {
  interrupt_handler_t handler;
  void *handler_arg;
} interrupt_handlers[INTERRUPT_HANDLER_MAX];

void
interrupt_attach(int irq, interrupt_handler_t handler, void *handler_arg)
{
  if ((irq < 0) || (irq >= INTERRUPT_HANDLER_MAX))
    k_panic("invalid interrupt id %d", irq);

  if (interrupt_handlers[irq].handler != NULL)
    k_panic("interrupt handler %d already attached", irq);

  interrupt_handlers[irq].handler     = handler;
  interrupt_handlers[irq].handler_arg = handler_arg;

  arch_interrupt_enable(irq, k_cpu_id());
  arch_interrupt_unmask(irq);
}

void
interrupt_attach_task(int irq, void (*handler)(int, void *), void *handler_arg)
{
  struct Page *stack_page;
  struct InterruptTask *isr;
  uint8_t *stack;

  if ((isr = k_malloc(sizeof(struct InterruptTask))) == NULL)
    k_panic("cannot allocate IRQ task strucure");

  if ((stack_page = page_alloc_one(0, PAGE_TAG_KSTACK)) == NULL)
    k_panic("cannot create IRQ task");

  stack = (uint8_t *) page2kva(stack_page);
  stack_page->ref_count++;

  if (k_task_create(&isr->task, NULL, interrupt_task_entry, isr, stack, PAGE_SIZE, 0) != 0)
    k_panic("cannot create IRQ task");

  k_semaphore_create(&isr->semaphore, 0);
  isr->irq         = irq;
  isr->handler     = handler;
  isr->handler_arg = handler_arg;

  interrupt_attach(irq, interrupt_task_notify, isr);

  k_task_resume(&isr->task);
}

void
interrupt_dispatch(struct TrapFrame *tf)
{
  int irq = arch_interrupt_id(tf);
  int should_unmask;

  k_irq_handler_begin();

  arch_interrupt_mask(irq);

  arch_interrupt_eoi(irq);

  should_unmask = interrupt_handler_call(irq);
  if (should_unmask)
    arch_interrupt_unmask(irq);

  k_irq_handler_end();
}

static int
interrupt_handler_call(int irq)
{
  if (interrupt_handlers[irq].handler)
    return interrupt_handlers[irq].handler(irq,
                                           interrupt_handlers[irq].handler_arg);
 
  // TODO: warn
  k_warn("Unexpected IRQ %d from CPU %d\n", irq, k_cpu_id());

  return 1;
}

static void
interrupt_task_entry(void *arg)
{
  struct InterruptTask *isr = (struct InterruptTask *) arg;

  for (;;) {
    if (k_semaphore_get(&isr->semaphore, K_SLEEP_UNINTERUPTIBLE) < 0)
      k_panic("k_semaphore_get");

    isr->handler(isr->irq, isr->handler_arg);
  }
}

static int
interrupt_task_notify(int irq, void *arg)
{
  struct InterruptTask *isr = (struct InterruptTask *) arg;

  (void) irq;
  k_semaphore_put(&isr->semaphore);

  // Do not re-enable the interrupt now, the handler task will do it
  return 0;
}

// TODO: detach
