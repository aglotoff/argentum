#ifndef KERNEL_KDEBUG_H
#define KERNEL_KDEBUG_H

#include <stdint.h>

struct PcDebugInfo {
  char *file;
  char *fn_name;
  unsigned line;
  uintptr_t fn_addr;
};

int debug_info_pc(uintptr_t pc, struct PcDebugInfo *info);

#endif  // KERNEL_KDEBUG_H
