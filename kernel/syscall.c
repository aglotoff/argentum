#include <errno.h>
#include <syscall.h>

#include "console.h"
#include "process.h"
#include "syscall.h"
#include "trap.h"

static int sys_fetch_int(uintptr_t, int *);
static int sys_fetch_str(uintptr_t, char **);
static int sys_get_num(void);
static int sys_get_arg(int);

static int (*syscalls[])(void) = {
  [SYS_cputs] = sys_cputs,
  [SYS_exit]  = sys_exit,
};

#define NSYSCALLS ((int) ((sizeof syscalls) / (sizeof syscalls[0])))

int
syscall(void)
{
  int num;

  if ((num = sys_get_num()) < 0)
    return num;

  if (num < NSYSCALLS && syscalls[num])
    return syscalls[num]();
  
  cprintf("Unknown system call %d\n");
  return -ENOSYS;
}

// --------------------------------------------------------------
// 
// --------------------------------------------------------------

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

static int
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
    return -EINVAL;
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
sys_arg_int(int n, int *ip)
{
  int r;

  if ((r = sys_get_arg(n)) < 0)
    return r;
  *ip = r;
  return 0;
}

int
sys_arg_ptr(int n, void **pp, size_t len)
{
  // TODO: implement this function!
  (void) n;
  (void) pp;
  (void) len;
  return -EINVAL;
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
int
sys_arg_str(int n, char **strp)
{
  int r;

  if ((r = sys_get_arg(n)) < 0)
    return r;
  return sys_fetch_str(r, strp);
}

// --------------------------------------------------------------
// System call implementations
// --------------------------------------------------------------

int
sys_cputs(void)
{
  char *s;
  int r;

  if ((r = sys_arg_str(0, &s)) < 0)
    return r;

  cprintf("CPU%d PID %d: %s", cpuid(), myprocess()->pid, s);
  return 0;
}

int
sys_exit(void)
{
  process_destroy();
  return 0;
}
