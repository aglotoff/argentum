#include <kernel/core/assert.h>
#include <arch/arm/mach.h>

// These symbols are defined by the linker script kernel.ld
extern struct Machine __mach_begin__[], __mach_end__[];

struct Machine *mach_current = NULL;

void
mach_init(uint32_t mach_type)
{
  struct Machine *m;

  for (m = __mach_begin__; m < __mach_end__; m++) {
    if (m->type == mach_type) {
      mach_current = m;
      return;
    }
  }
  
  k_panic("unknown machine type %x", mach_type);
}
