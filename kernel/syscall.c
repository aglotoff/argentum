#include <assert.h>
#include <errno.h>
#include <syscall.h>

#include "kernel.h"
#include "console.h"
#include "cpu.h"
#include "process.h"
#include "rtc.h"
#include "syscall.h"
#include "trap.h"
#include "vm.h"

static int     sys_get_num(void);
static int32_t sys_get_arg(int);

static int32_t (*syscalls[])(void) = {
  [__SYS_CWRITE]  = sys_cwrite,
  [__SYS_EXIT]    = sys_exit,
  [__SYS_GETPID]  = sys_getpid,
  [__SYS_GETPPID] = sys_getppid,
  [__SYS_TIME]    = sys_time,
  [__SYS_FORK]    = sys_fork,
  [__SYS_WAIT]    = sys_wait,
  [__SYS_EXEC]    = sys_exec,
};

int32_t
sys_dispatch(void)
{
  int num;

  if ((num = sys_get_num()) < 0)
    return num;

  if ((num < (int) ARRAY_SIZE(syscalls)) && syscalls[num])
    return syscalls[num]();

  cprintf("Unknown system call %d\n", cpu_id(), num);
  return -ENOSYS;
}

/*
 * ----------------------------------------------------------------------------
 * Helper functions
 * ----------------------------------------------------------------------------
 */

// Extract system call number from the SVC instruction opcode
static int
sys_get_num(void)
{
  struct Process *current = my_process();
  int *pc = (int *) (current->tf->pc - 4);
  int r;

  if ((r = vm_check_user_ptr(current->trtab, pc, sizeof(int), AP_USER_RO)) < 0)
    return r;

  return *pc & 0xFFFFFF;
}

// Get the n-th argument from the current process' trap frame.
static int32_t
sys_get_arg(int n)
{
  struct Process *current = my_process();

  switch (n) {
  case 0:
    return current->tf->r0;
  case 1:
    return current->tf->r1;
  case 2:
    return current->tf->r2;
  case 3:
    return current->tf->r3;
  default:
    if (n < 0)
      panic("Invalid argument number: %d", n);

    // TODO: grab additional parameters from the user's stack.
    return 0;
  }
}

/**
 * Fetch the nth system call argument as an integer.
 * 
 * @param n The argument number.
 * @param ip Pointer to the memory address to store the argument value.
 * 
 * @retval 0 on success.
 * @retval -EINVAL if an invalid argument number is specified.
 */
int
sys_arg_int(int n, int32_t *ip)
{
  *ip = sys_get_arg(n);
  return 0;
}

/**
 * Fetch the nth system call argument as pointer to a buffer of the specified
 * length. Check that the pointer is valid and the user has right permissions.
 * 
 * @param n    The argument number.
 * @param pp   Pointer to the memory address to store the argument value.
 * @param len  The length of the memory region pointed to.
 * @param perm The permissions to be checked.
 * 
 * @retval 0 on success.
 * @retval -EFAULT if the arguments doesn't point to a valid memory region.
 */
int32_t
sys_arg_ptr(int n, void **pp, size_t len, int perm)
{ 
  void *ptr = (void *) sys_get_arg(n);
  int r;

  if ((r = vm_check_user_ptr(my_process()->trtab, ptr, len, perm)) < 0)
    return r;

  *pp = ptr;

  return 0;
}

/**
 * Fetch the nth system call argument as a string pointer. Check that the
 * pointer is valid, the user has right permissions and the string is
 * null-terminated.
 * 
 * @param n    The argument number.
 * @param strp Pointer to the memory address to store the argument value.
 * @param perm The permissions to be checked.
 * 
 * @retval 0 on success.
 * @retval -EFAULT if the arguments doesn't point to a valid string.
 */
int32_t
sys_arg_str(int n, const char **strp, int perm)
{
  char *str = (char *) sys_get_arg(n);
  int r;

  if ((r = vm_check_user_str(my_process()->trtab, str, perm)) < 0)
    return r;

  *strp = str;

  return 0;
}

/*
 * ----------------------------------------------------------------------------
 * System call implementations
 * ----------------------------------------------------------------------------
 */

int32_t
sys_cwrite(void)
{
  const char *s;
  int32_t n;
  int r;

  if ((r = sys_arg_int(1, &n)) < 0)
    return r;

  if ((r = sys_arg_ptr(0, (void **) &s, n, AP_USER_RO)) < 0)
    return r;

  cprintf("%.*s", n, s);

  return n;
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

int32_t
sys_fork(void)
{
  return process_copy();
}

int32_t
sys_wait(void)
{
  pid_t pid;
  int *stat_loc;
  int r;
  
  if ((r = sys_arg_int(0, &pid)) < 0)
    return r;

  if ((r = sys_arg_ptr(1, (void **) &stat_loc, sizeof(int), AP_BOTH_RW)) < 0)
    return r;

  return process_wait(pid, stat_loc, 0);
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
