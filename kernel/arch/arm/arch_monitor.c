#include <stddef.h>


#include <kernel/monitor.h>
#include <kernel/trap.h>
#include <kernel/kdebug.h>
#include <kernel/console.h>

#include <arch/arm/regs.h>

void
arch_mon_backtrace(struct TrapFrame *tf)
{
  struct PcDebugInfo info;
  uint32_t *fp;

  fp = (uint32_t *) (tf ? tf->r11 : r11_get());
  for ( ; fp != NULL; fp = (uint32_t *) fp[APCS_FRAME_FP]) {
    debug_info_pc(fp[-1], &info);

    cprintf("  [%p] %s (%s at line %d)\n",
            fp[-1], info.fn_name, info.file, info.line);
  }
}
