#ifndef __KERNEL_INCLUDE_KERNEL__KDEBUG_H__
#define __KERNEL_INCLUDE_KERNEL__KDEBUG_H__

#ifndef __OSDEV_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <stdint.h>

struct PcDebugInfo {
  char *file;
  char *fn_name;
  unsigned line;
  uintptr_t fn_addr;
};

int debug_info_pc(uintptr_t pc, struct PcDebugInfo *info);

#endif  // __KERNEL_INCLUDE_KERNEL__KDEBUG_H__
