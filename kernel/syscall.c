#include <argentum/syscall.h>
#include <errno.h>

#include <kernel/kernel.h>
#include <kernel/syscall.h>
#include <kernel/thread.h>
#include <kernel/vm.h>

static long sys_exit(void *);
static long sys_cwrite(void *);

static long (*syscalls[])(void *) = {
  [__SYS_EXIT]   = sys_exit,
  [__SYS_CWRITE] = sys_cwrite,
};

long
syscall_dispatch(void *ctx)
{
  int num;

  if ((num = arch_syscall_no(ctx)) < 0)
    return num;

  if ((num < (int) ARRAY_SIZE(syscalls)) && syscalls[num])
    return syscalls[num](ctx);

  panic("Unkown");
  return -ENOSYS;
}

static long
sys_exit(void *ctx)
{
  int status = arch_syscall_arg(ctx, 0);

  thread_exit(status);
  return 0;
}

static long
sys_cwrite(void *ctx)
{
  uintptr_t src = arch_syscall_arg(ctx, 0);
  size_t n = arch_syscall_arg(ctx, 1);

  char buf[64];

  while (n != 0) {
    size_t ncopy = MIN(n, sizeof buf);
    int err;

    if ((err = vm_copy_in(thread_current()->process->vm, src, buf, ncopy)) < 0)
      return err;

    kprintf("%.*s", ncopy, buf);

    n   -= ncopy;
    src += ncopy;
  } 
  
  return 0;
}