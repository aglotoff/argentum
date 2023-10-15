
#include <smp.h>
#include <irq.h>
#include <kernel.h>
#include <spinlock.h>

static struct {
  struct ListLink list;
  struct SpinLock lock;
  int             active_ids;
} irq_hooks[IRQ_MAX];

void
irq_init(void)
{
  int i;

  for (i = 0; i < IRQ_MAX; i++) {
    list_init(&irq_hooks[i].list);
    spin_init(&irq_hooks[i].lock, "irq_hooks");
  }
}

/**
 * Save the current CPU interrupt state and disable interrupts.
 */
void
irq_save(void)
{
  int flags = arch_irq_save();

  if (smp_cpu()->irq_save_count++ == 0)
    smp_cpu()->irq_flags = flags;
}

/**
 * Restore the interrupt state saved by a preceding cpu_irq_save() call.
 */
void
irq_restore(void)
{
  if (arch_irq_is_enabled())
    panic("interruptible");

  if (--smp_cpu()->irq_save_count < 0)
    panic("interruptible");

  if (smp_cpu()->irq_save_count == 0)
    arch_irq_restore(smp_cpu()->irq_flags);
}

int
irq_hook_attach(struct IrqHook *hook, int irq, int (*handler)(int))
{
  struct ListLink *l;
  int used_mask, id;
  
  if ((irq < 0) || (irq >= IRQ_MAX))
    return -1;

  spin_lock(&irq_hooks[irq].lock);

  used_mask = 0;
  LIST_FOREACH(&irq_hooks[irq].list, l) {
    struct IrqHook *hook = LIST_CONTAINER(l, struct IrqHook, link);
    used_mask |= hook->id;
  }

  for (id = 1; id != 0; id <<= 1)
    if (!(used_mask & id))
      break;

  if (id == 0) {
    spin_unlock(&irq_hooks[irq].lock);
    return -1;
  }

  hook->id      = id;
  hook->irq     = irq;
  hook->handler = handler;

  list_add_back(&irq_hooks[irq].list, &hook->link);

  arch_irq_unmask(irq);

  spin_unlock(&irq_hooks[irq].lock);

  return id;
}

int
irq_hook_enable(struct IrqHook *hook)
{
  int irq = hook->irq;

  if ((irq < 0) || (irq >= IRQ_MAX))
    return -1;

  spin_lock(&irq_hooks[irq].lock);

  irq_hooks[irq].active_ids &= ~hook->id;

  if (!irq_hooks[irq].active_ids)
    arch_irq_unmask(irq);

  spin_unlock(&irq_hooks[irq].lock);

  return 0;
}

int
irq_hook_disable(struct IrqHook *hook)
{
  int irq = hook->irq;

  if ((irq < 0) || (irq >= IRQ_MAX))
    return -1;

  spin_lock(&irq_hooks[irq].lock);

  list_remove(&hook->link);
  irq_hooks[irq].active_ids |= hook->id;

  arch_irq_mask(hook->irq);

  spin_unlock(&irq_hooks[irq].lock);

  return 0;
}

void
irq_handle(int irq)
{
  struct ListLink *l;

  arch_irq_mask(irq);

  spin_lock(&irq_hooks[irq].lock);

  LIST_FOREACH(&irq_hooks[irq].list, l) {
    struct IrqHook *hook = LIST_CONTAINER(l, struct IrqHook, link);
    int id = hook->id;
    
    if (!(irq_hooks[irq].active_ids & id)) {
      irq_hooks[irq].active_ids |= id;

      if (hook->handler(irq))
        irq_hooks[irq].active_ids &= ~id;
    }
  }

  if (!irq_hooks[irq].active_ids)
    arch_irq_unmask(irq);

  spin_unlock(&irq_hooks[irq].lock);
}
