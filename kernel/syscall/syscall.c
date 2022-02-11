#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <syscall.h>
#include <sys/stat.h>

#include <kernel/cpu.h>
#include <kernel/drivers/console.h>
#include <kernel/mm/vm.h>
#include <kernel/process.h>
#include <kernel/types.h>

#include <kernel/syscall.h>

static int     sys_get_num(void);
static int32_t sys_get_arg(int);

static int32_t (*syscalls[])(void) = {
  [__SYS_FORK]     = sys_fork,
  [__SYS_EXEC]     = sys_exec,
  [__SYS_WAIT]     = sys_wait,
  [__SYS_EXIT]     = sys_exit,
  [__SYS_GETPID]   = sys_getpid,
  [__SYS_GETPPID]  = sys_getppid,
  [__SYS_TIME]     = sys_time,
  [__SYS_GETDENTS] = sys_getdents,
  [__SYS_CHDIR]    = sys_chdir,
  [__SYS_OPEN]     = sys_open,
  [__SYS_MKDIR]    = sys_mkdir,
  [__SYS_MKNOD]    = sys_mknod,
  [__SYS_UNLINK]   = sys_unlink,
  [__SYS_RMDIR]    = sys_rmdir,
  [__SYS_STAT]     = sys_stat,
  [__SYS_CLOSE]    = sys_close,
  [__SYS_READ]     = sys_read,
  [__SYS_WRITE]    = sys_write,
  [__SYS_SBRK]     = sys_sbrk,
};

int32_t
sys_dispatch(void)
{
  int num;

  if ((num = sys_get_num()) < 0)
    return num;

  if ((num < (int) ARRAY_SIZE(syscalls)) && syscalls[num]) {
    int r = syscalls[num]();
    // cprintf("SYS(%d) -> %d, pc = %p\n", num, r, my_process()->tf->pc);
    return r;
  }

  cprintf("Unknown system call %d\n", cpu_id(), num);
  return -ENOSYS;
}

/*
 * ----------------------------------------------------------------------------
 * Helper functions
 * ----------------------------------------------------------------------------
 */

// Extract the system call number from the SVC instruction opcode
static int
sys_get_num(void)
{
  struct Process *current = my_process();
  int *pc = (int *) (current->tf->pc - 4);
  int r;

  if ((r = vm_check_user_ptr(current->trtab, pc, sizeof(int), VM_U)) < 0)
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
sys_arg_int(int n, int *ip)
{
  *ip = sys_get_arg(n);
  return 0;
}

/**
 * Fetch the nth system call argument as a long integer.
 * 
 * @param n The argument number.
 * @param ip Pointer to the memory address to store the argument value.
 * 
 * @retval 0 on success.
 * @retval -EINVAL if an invalid argument number is specified.
 */
int
sys_arg_short(int n, short *ip)
{
  *ip = (short) sys_get_arg(n);
  return 0;
}

int
sys_arg_long(int n, long *ip)
{
  *ip = (long) sys_get_arg(n);
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
sys_arg_buf(int n, void **pp, size_t len, int perm)
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

/**
 * Fetch the nth system call argument as a file descriptor. Check that the
 * file desriptor is valid.
 * 
 * @param n       The argument number.
 * @param fdstore Pointer to the memory address to store the file descriptor.
 * @param fstore  Pointer to the memory address to store the file.
 * 
 * @retval 0 on success.
 * @retval -EFAULT if the arguments doesn't point to a valid string.
 * @retval -EBADF if the file descriptor is invalid.
 */
int
sys_arg_fd(int n, int *fdstore, struct File **fstore)
{
  struct Process *current = my_process();
  int fd = sys_get_arg(n);

  if ((fd < 0) || (fd >= OPEN_MAX) || (current->files[fd] == NULL))
    return -EBADF;
  
  if (fdstore)
    *fdstore = fd;
  if (fstore)
    *fstore = current->files[fd];

  return 0;
}

int
sys_arg_args(int n, char ***store)
{
  struct Process *current = my_process();
  char **args;
  int i;

  args = (char **) sys_get_arg(n);
  
  for (i = 0; ; i++) {
    int r;

    if ((r = vm_check_user_ptr(current->trtab, args + i, sizeof(args[i]),
                               VM_U)) < 0)
      return r;

    if (args[i] == NULL)
      break;
    
    if ((r = vm_check_user_str(current->trtab, args[i], VM_U)) < 0)
      return r;
  }

  if (store)
    *store = args;

  return 0;
}
