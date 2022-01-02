#include <errno.h>
#include <syscall.h>

#include "kernel.h"
#include "console.h"
#include "process.h"
#include "sbcon.h"
#include "syscall.h"
#include "trap.h"

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
};

int32_t
sys_dispatch(void)
{
  int num;

  if ((num = sys_get_num()) < 0)
    return num;

  if ((num < (int) ARRAY_SIZE(syscalls)) && syscalls[num])
    return syscalls[num]();

  cprintf("[CPU %d: Unknown system call %d]\n", cpuid(), num);
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
  struct Process *curproc = myprocess();

  if (addr >= curproc->size || (addr + sizeof(int)) > curproc->size)
    return -EFAULT;
  *ip = *(int *) addr;
  return 0;
}

// Fetch the null-terminated string at addr from the current process.
static int
sys_fetch_str(uintptr_t addr, char **strp)
{
  char *s, *end;
  struct Process *curproc = myprocess();

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

  if ((r = sys_fetch_int(myprocess()->tf->pc - 4, &svc_code)) < 0)
    return r;
  return svc_code & 0xFFFFFF;
}

static int32_t
sys_get_arg(int n)
{
  struct Process *curproc = myprocess();

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
  int32_t r;

  if ((r = sys_get_arg(n)) < 0)
    return r;
  *ip = r;
  return 0;
}

int32_t
sys_arg_ptr(int n, void **pp, size_t len)
{ 
  // TODO: implement this function!
  (void) len;

  *pp = (void *) sys_get_arg(n);

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

  if ((r = sys_arg_ptr(0, (void **) &s, n)) < 0)
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
  return myprocess()->pid;
}

int32_t
sys_getppid(void)
{
  return myprocess()->parent ? myprocess()->parent->pid : myprocess()->pid;
}

int32_t
sys_time(void)
{
  return sb_rtc_time();
}

int32_t
sys_fork(void)
{
  return process_copy();
}
