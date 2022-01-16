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

static int     sys_fetch_int(uintptr_t, int *);
static int     sys_fetch_str(uintptr_t, char **);
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
};

int32_t
sys_dispatch(void)
{
  int num;

  if ((num = sys_get_num()) < 0)
    return num;

  if ((num < (int) ARRAY_SIZE(syscalls)) && syscalls[num])
    return syscalls[num]();

  cprintf("[CPU %d: Unknown system call %d]\n", cpu_id(), num);
  return -ENOSYS;
}

/*
 * ----------------------------------------------------------------------------
 * Helper functions
 * ----------------------------------------------------------------------------
 */

// Fetch the integer at addr from the current process.
static int
sys_fetch_int(uintptr_t addr, int *ip)
{
  struct Process *curr = my_process();
  int r;

  if ((r = vm_check(curr->trtab, (void *) addr, sizeof(int), AP_USER_RO)) < 0)
    return r;

  *ip = *(int *) addr;

  return 0;
}

// Fetch the null-terminated string at addr from the current process.
static int
sys_fetch_str(uintptr_t addr, char **strp)
{
  char *s, *end;
  struct Process *curproc = my_process();

  if (addr >= curproc->size)
    return -EFAULT;
  
  for (s = (char *) addr, end = (char *) curproc->size; s < end; s++) {
    if (*s == '\0') {
      *strp = (char *) addr;
      return s - *strp;
    }
  }
  return -EFAULT;
}

static int
sys_get_num(void)
{
  int svc_code, r;

  if ((r = sys_fetch_int(my_process()->tf->pc - 4, &svc_code)) < 0)
    return r;
  return svc_code & 0xFFFFFF;
}

static int32_t
sys_get_arg(int n)
{
  struct Process *curproc = my_process();

  switch (n) {
  case 0:
    return curproc->tf->r0;
  case 1:
    return curproc->tf->r1;
  case 2:
    return curproc->tf->r2;
  default:
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

int32_t
sys_arg_ptr(int n, void **pp, size_t len, int perm)
{ 
  struct Process *curproc = my_process();
  void *ptr = (void *) sys_get_arg(n);
  int r;

  if ((r = vm_check(curproc->trtab, ptr, len, perm)) < 0)
    return r;

  *pp = ptr;

  return 0;
}

/**
 * Fetch the nth system call argument as a string pointer. Check that the
 * pointer is valid and the string is null-terminated.
 * 
 * @param n The argument number.
 * @param ip Pointer to the memory address to store the argument value.
 * 
 * @retval 0 on success.
 * @retval -EINVAL if an invalid argument number is specified.
 * @retval -EFAULT if the arguments doesn't point to a valid string.
 */
int32_t
sys_arg_str(int n, char **strp)
{
  int r;

  if ((r = sys_get_arg(n)) < 0)
    return r;
  return sys_fetch_str(r, strp);
}

/*
 * ----------------------------------------------------------------------------
 * System call implementations
 * ----------------------------------------------------------------------------
 */

int32_t
sys_cwrite(void)
{
  char *s;
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
