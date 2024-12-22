#include <kernel/process.h>
#include <kernel/sys.h>
#include <kernel/vmspace.h>

int
sys_arch_get_num(void)
{
  // TODO
  return -1;
}

int32_t
sys_arch_get_arg(int n)
{
  // TODO
  (void) n;
  return -1;
}
