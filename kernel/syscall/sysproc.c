#include <errno.h>

#include <kernel/drivers/console.h>
#include <kernel/drivers/rtc.h>
#include <kernel/syscall.h>
#include <kernel/mm/vm.h>
#include <kernel/process.h>

int32_t
sys_fork(void)
{
  return process_copy();
}

int32_t
sys_exec(void)
{
  struct Process *current = my_process();
  const char *path;
  char **argv, **s;
  int i, r;

  if ((r = sys_arg_str(0, &path, AP_USER_RO)) < 0)
    return r;

  if ((r = sys_arg_int(1, (int *) &argv)) < 0)
    return r;

  for (i = 0; ; i++) {
    s = argv + i;

    if ((r = vm_check_user_ptr(current->trtab, s, sizeof(*s), AP_USER_RO)) < 0)
      return r;

    if (*s == NULL)
      break;

    if (i >= 32)
      return -E2BIG;

    if ((r = vm_check_user_str(current->trtab, *s, AP_USER_RO)))
      return r;
  }

  return process_exec(path, argv);
}

int32_t
sys_wait(void)
{
  pid_t pid;
  int *stat_loc;
  int r;
  
  if ((r = sys_arg_int(0, &pid)) < 0)
    return r;

  if ((r = sys_arg_buf(1, (void **) &stat_loc, sizeof(int), AP_BOTH_RW)) < 0)
    return r;

  return process_wait(pid, stat_loc, 0);
}

int32_t
sys_exit(void)
{
  int status, r;
  
  if ((r = sys_arg_int(0, &status)) < 0)
    return r;

  process_destroy(status);

  return 0;
}

int32_t
sys_getpid(void)
{
  return my_process()->pid;
}

int32_t
sys_getppid(void)
{
  return my_process()->parent ? my_process()->parent->pid : my_process()->pid;
}

int32_t
sys_time(void)
{
  return rtc_time();
}
