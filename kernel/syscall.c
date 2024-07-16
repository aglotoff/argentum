#include <kernel/assert.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/utsname.h>
#include <time.h>

#include <kernel/cprintf.h>
#include <kernel/cpu.h>
#include <kernel/drivers/rtc.h>
#include <kernel/fd.h>
#include <kernel/fs/file.h>
#include <kernel/fs/fs.h>
#include <kernel/vmspace.h>
#include <kernel/net.h>
#include <kernel/pipe.h>
#include <kernel/process.h>
#include <kernel/sys.h>
#include <kernel/types.h>
#include <kernel/object_pool.h>
#include <kernel/irq.h>

#include <lwip/sockets.h>

static int     sys_arch_get_num(void);
static int32_t sys_arch_get_arg(int);

static int32_t (*syscalls[])(void) = {
  [__SYS_FORK]        = sys_fork,
  [__SYS_EXEC]        = sys_exec,
  [__SYS_WAIT]        = sys_wait,
  [__SYS_EXIT]        = sys_exit,
  [__SYS_GETPID]      = sys_getpid,
  [__SYS_GETPPID]     = sys_getppid,
  [__SYS_GETDENTS]    = sys_getdents,
  [__SYS_CHDIR]       = sys_chdir,
  [__SYS_FCHDIR]      = sys_fchdir,
  [__SYS_OPEN]        = sys_open,
  [__SYS_FCNTL]       = sys_fcntl,
  [__SYS_SEEK]        = sys_seek,
  [__SYS_UMASK]       = sys_umask,
  [__SYS_MKNOD]       = sys_mknod,
  [__SYS_LINK]        = sys_link,
  [__SYS_UNLINK]      = sys_unlink,
  [__SYS_RMDIR]       = sys_rmdir,
  [__SYS_STAT]        = sys_stat,
  [__SYS_CLOSE]       = sys_close,
  [__SYS_READ]        = sys_read,
  [__SYS_WRITE]       = sys_write,
  [__SYS_SBRK]        = sys_sbrk,
  [__SYS_UNAME]       = sys_uname,
  [__SYS_CHMOD]       = sys_chmod,
  [__SYS_CLOCK_TIME]  = sys_clock_time,
  [__SYS_SOCKET]      = sys_socket,
  [__SYS_BIND]        = sys_bind,
  [__SYS_LISTEN]      = sys_listen,
  [__SYS_ACCEPT]      = sys_accept,
  [__SYS_CONNECT]     = sys_connect,
  [__SYS_TEST]        = sys_test,
  [__SYS_FCHMOD]      = sys_fchmod,
  [__SYS_SIGACTION]   = sys_sigaction,
  [__SYS_SIGRETURN]   = sys_sigreturn,
  [__SYS_SIGPENDING]  = sys_sigpending,
  [__SYS_SIGPROCMASK] = sys_sigprocmask,
  [__SYS_NANOSLEEP]   = sys_nanosleep,
  [__SYS_RECVFROM]    = sys_recvfrom,
  [__SYS_SENDTO]      = sys_sendto,
  [__SYS_SETSOCKOPT]  = sys_setsockopt,
  [__SYS_GETUID]      = sys_getuid,
  [__SYS_GETEUID]     = sys_geteuid,
  [__SYS_GETGID]      = sys_getgid,
  [__SYS_GETEGID]     = sys_getegid,
  [__SYS_GETPGID]     = sys_getpgid,
  [__SYS_SETPGID]     = sys_setpgid,
  [__SYS_ACCESS]      = sys_access,
  [__SYS_PIPE]        = sys_pipe,
  [__SYS_IOCTL]       = sys_ioctl,
  [__SYS_MMAP]        = sys_mmap,
  [__SYS_SELECT]      = sys_select,
  [__SYS_SIGSUSPEND]  = sys_sigsuspend,
  [__SYS_KILL]        = sys_kill,
  [__SYS_FSYNC]       = sys_fsync,
  [__SYS_FTRUNCATE]   = sys_ftruncate,
  [__SYS_FCHOWN]      = sys_fchown,
  [__SYS_READLINK]    = sys_readlink,
  [__SYS_TIMES]       = sys_times,
  [__SYS_MOUNT]       = sys_mount,
};

int32_t
sys_dispatch(void)
{
  int num;

  if ((num = sys_arch_get_num()) < 0)
    return num;

  if ((num < (int) ARRAY_SIZE(syscalls)) && syscalls[num]) {

    int r = syscalls[num]();

    // if (r < 0)
    //   cprintf("syscall(%d) -> %d\n", num, r);
    return r;
  }

  //cprintf("Unknown system call %d\n", num);
  return -ENOSYS;
}

/*
 * ----------------------------------------------------------------------------
 * Helper functions
 * ----------------------------------------------------------------------------
 */

// Extract the system call number from the SVC instruction opcode
static int
sys_arch_get_num(void)
{
  struct Process *current = process_current();

  int *pc = (int *) (current->thread->tf->pc - 4);
  int r;

  if ((r = vm_user_check_buf(current->vm->pgtab, (uintptr_t) pc, sizeof(int), VM_READ)) < 0)
    return r;

  return *pc & 0xFFFFFF;
}

// Get the n-th argument from the current process' trap frame.
// Support up to 6 system call arguments
static int32_t
sys_arch_get_arg(int n)
{
  struct Process *current = process_current();

  switch (n) {
  case 0:
    return current->thread->tf->r0;
  case 1:
    return current->thread->tf->r1;
  case 2:
    return current->thread->tf->r2;
  case 3:
    return current->thread->tf->r3;
  case 4:
    return current->thread->tf->r4;
  case 5:
    return current->thread->tf->r5;
  default:
    panic("Invalid argument number: %d", n);
    return 0;
  }
}

static int
sys_arg_int(int n, int *ip)
{
  *ip = sys_arch_get_arg(n);
  return 0;
}

static int
sys_arg_uint(int n, unsigned int *ip)
{
  *ip = sys_arch_get_arg(n);
  return 0;
}

static int
sys_arg_short(int n, short *ip)
{
  *ip = (short) sys_arch_get_arg(n);
  return 0;
}

static int
sys_arg_ushort(int n, unsigned short *ip)
{
  *ip = (unsigned short) sys_arch_get_arg(n);
  return 0;
}

static int
sys_arg_long(int n, long *ip)
{
  *ip = (long) sys_arch_get_arg(n);
  return 0;
}

static int
sys_arg_ulong(int n, unsigned long *ip)
{
  *ip = (unsigned long) sys_arch_get_arg(n);
  return 0;
}

static int
sys_arg_ptr(int n, uintptr_t *pp, int perm, int can_be_null)
{
  uintptr_t ptr = sys_arch_get_arg(n);
  int r;

  if (ptr == 0) {
    if (can_be_null) {
      *pp = 0;
      return 0;
    }
    return -EFAULT;
  }

  if ((r = vm_user_check_ptr(process_current()->vm->pgtab, ptr, perm)) < 0)
    return r;

  *pp = ptr;

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
static int32_t
sys_arg_va(int n, uintptr_t *pp, size_t len, int perm, int can_be_null)
{ 
  uintptr_t ptr = sys_arch_get_arg(n);
  int r;

  if (!ptr) {
    if (can_be_null) {
      *pp = 0;
      return 0;
    }
    return -EFAULT;
  }

  if ((r = vm_user_check_buf(process_current()->vm->pgtab, ptr, len, perm)) < 0)
    return r;

  *pp = ptr;

  return 0;
}

static int32_t
sys_arg_buf(int n, void **store, size_t len, int perm)
{ 
  uintptr_t va = sys_arch_get_arg(n);
  void *pgtab = process_current()->vm->pgtab;
  void *p;
  int r;

  if (va == 0) {
    *store = NULL;
    return 0;
  }

  if ((r = vm_user_check_buf(pgtab, va, len, perm | VM_USER)) < 0)
    return r;

  if ((p = k_malloc(len)) == NULL)
    return -ENOMEM;

  if ((r = vm_copy_in(pgtab, p, va, len)) < 0) {
    k_free(p);
    return r;
  }

  *store = p;

  return 0;
}

// Fetch the nth system call argument as a C string pointer. Check that the
// pointer is valid, the user has right permissions and the string is properly
// terminated. If strp is not null, allocate a temporary buffer to copy the
// string into. Tha caller must deallocate this buffer by calling k_free().
static int32_t
sys_arg_str(int n, size_t max, int perm, char **strp)
{
  uintptr_t va = sys_arch_get_arg(n);
  void *pgtab = process_current()->vm->pgtab;
  size_t len;
  char *s;
  int r;

  if ((r = vm_user_check_str(pgtab, va, &len, perm)) < 0)
    return r;

  if (len >= max)
    return -ENAMETOOLONG;

  if (strp == NULL)
    return 0;

  if ((s = k_malloc(len + 1)) == NULL)
    return -ENOMEM;

  if ((vm_copy_in(pgtab, s, va, len + 1) != 0) || (s[len] != '\0')) {
    k_free(s);
    return -EFAULT;
  }

  *strp = s;

  return 0;
}

static void
sys_free_args(char **args)
{
  char **p;

  for (p = args; *p != NULL; p++)
    k_free(*p);

  k_free(args);
}

static int
sys_arg_args(int n, char ***store, size_t *len_store)
{
  void *pgtab = process_current()->vm->pgtab;
  uintptr_t va = sys_arch_get_arg(n);
  char **args;
  size_t len;
  size_t total_len;
  int r;

  if ((vm_user_check_args(pgtab, va, &len, VM_READ | VM_USER)) < 0)
    return r;
  
  total_len = (len + 1) * sizeof(char *);
  if (total_len > ARG_MAX)
    return -E2BIG;

  if ((args = (char **) k_malloc(total_len)) == NULL)
    return -ENOMEM;

  memset(args, 0, total_len);

  for (size_t i = 0; i < len; i++) {
    uintptr_t str_va;
    size_t str_len;

    if ((r = vm_copy_in(pgtab, &str_va, va + (sizeof(char *)*i), sizeof str_va)) < 0) {
      sys_free_args(args);
      return r;
    }

    if ((r = vm_user_check_str(pgtab, str_va, &str_len, VM_READ | VM_USER)) < 0) {
      sys_free_args(args);
      return r;
    }

    total_len += str_len + 1;
    if (total_len > ARG_MAX) {
      sys_free_args(args);
      return -E2BIG;
    }

    if ((args[i] = k_malloc(str_len + 1)) == NULL) {
      sys_free_args(args);
      return -ENOMEM;
    }

    if ((vm_copy_in(pgtab, args[i], str_va, str_len + 1) != 0) || (args[i][str_len] != '\0')) {
      sys_free_args(args);
      return -EFAULT;
    }
  }

  *len_store = total_len;
  *store = args;

  return 0;
}

static int
sys_copy_out(const void *src, uintptr_t va, size_t n)
{
  return vm_copy_out(process_current()->vm->pgtab, src, va, n);
}

/*
 * ----------------------------------------------------------------------------
 * Process system calls
 * ----------------------------------------------------------------------------
 */

int32_t
sys_fork(void)
{
  int r, shared;

  if ((r = sys_arg_int(0, &shared)) < 0)
    return r;

  return process_copy(0);
}

int32_t
sys_exec(void)
{
  char *path;
  char **argv, **envp;
  size_t argv_len, envp_len;
  int r;

  if ((r = sys_arg_str(0, PATH_MAX, VM_READ, &path)) < 0)
    goto out1;
  if ((r = sys_arg_args(1, &argv, &argv_len)) < 0)
    goto out2;
  if ((r = sys_arg_args(2, &envp, &envp_len)) < 0)
    goto out3;

  if (argv_len + envp_len > ARG_MAX) {
    r = -E2BIG;
    goto out4;
  }

  r = process_exec(path, argv, envp);

out4:
  sys_free_args(envp);
out3:
  sys_free_args(argv);
out2:
  k_free(path);
out1:
  return r;
}

int32_t
sys_wait(void)
{
  pid_t pid;
  uintptr_t stat_va;
  int r, stat, options;
  
  if ((r = sys_arg_int(0, &pid)) < 0)
    return r;
  if ((r = sys_arg_va(1, &stat_va, sizeof(int), VM_WRITE, 1)) < 0)
    return r;
  if ((r = sys_arg_int(2, &options)) < 0)
    return r;

  if ((r = process_wait(pid, &stat, options)) < 0)
    return r;

  if (!stat_va)
    return r;

  pid = r;

  if ((r = sys_copy_out(&stat, stat_va, sizeof stat)) < 0)
    return r;

  return pid;
}

int32_t
sys_exit(void)
{
  int status, r;
  
  if ((r = sys_arg_int(0, &status)) < 0)
    return r;

  process_destroy((status & 0xFF) << 8);
  // Should not return
  return 0;
}

int32_t
sys_getpid(void)
{
  return process_current()->pid;
}

int32_t
sys_getuid(void)
{
  return process_current()->ruid;
}

int32_t
sys_geteuid(void)
{
  return process_current()->euid;
}

int32_t
sys_getgid(void)
{
  return process_current()->rgid;
}

int32_t
sys_getegid(void)
{
  return process_current()->egid;
}

int32_t
sys_getppid(void)
{
  struct Process *current = process_current(),
                 *parent  = current->parent;

  return parent ? parent->pid : current->pid;
}

int32_t
sys_umask(void)
{
  struct Process *proc = process_current();
  mode_t cmask;
  int r;

  if ((r = sys_arg_ulong(0, &cmask)) < 0)
    return r;

  r = proc->cmask & (S_IRWXU | S_IRWXG | S_IRWXO);
  proc->cmask = cmask;

  return r;
}

int32_t
sys_times(void)
{
  uintptr_t times_va;
  struct tms times;
  int r;

  if ((r = sys_arg_va(0, &times_va, sizeof times, VM_WRITE, 0)) < 0)
    return r;

  process_get_times(process_current(), &times);

  return sys_copy_out(&times, times_va, sizeof times);
}

int32_t
sys_nanosleep(void)
{
  struct timespec *rqtp, rmt;
  uintptr_t rmt_va;
  int r;

  if ((r = sys_arg_buf(0, (void **) &rqtp, sizeof *rqtp, VM_READ)) < 0)
    goto out1;
  if ((r = sys_arg_va(1, &rmt_va, sizeof rmt, VM_WRITE, 1)) < 0)
    goto out2;

  if ((r = process_nanosleep(rqtp, &rmt)) < 0)
    goto out2;

  if (rmt_va && ((r = sys_copy_out(&rmt, rmt_va, sizeof rmt)) < 0))
    goto out2;

out2:
  if (rqtp != NULL)
    k_free(rqtp);
out1:
  return r;
}

/*
 * ----------------------------------------------------------------------------
 * Path name system calls
 * ----------------------------------------------------------------------------
 */

int32_t
sys_mount(void)
{
  char *type, *path;
  int r;

  if ((r = sys_arg_str(0, PATH_MAX, VM_READ, &type)) < 0)
    goto out1;
  if ((r = sys_arg_str(1, PATH_MAX, VM_READ, &path)) < 0)
    goto out2;

  r = fs_mount(type, path);

  k_free(path);
out2:
  k_free(type);
out1:
  return r;
}

int32_t
sys_chdir(void)
{
  char *path;
  int r;

  if ((r = sys_arg_str(0, PATH_MAX, VM_READ, &path)) < 0)
    return r;

  r = fs_chdir(path);

  k_free(path);
  return r;
}

int32_t
sys_chmod(void)
{
  char *path;
  mode_t mode;
  int r;

  if ((r = sys_arg_str(0, PATH_MAX, VM_READ, &path)) < 0)
    goto out1;
  if ((r = sys_arg_ulong(1, &mode)) < 0)
    goto out2;

  r = fs_chmod(path, mode);

out2:
  k_free(path);
out1:
  return r;
}

int32_t
sys_open(void)
{
  struct File *file;
  char *path;
  int oflag, r;
  mode_t mode;

  if ((r = sys_arg_str(0, PATH_MAX, VM_READ, &path)) < 0)
    goto out1;
  if ((r = sys_arg_int(1, &oflag)) < 0)
    goto out2;
  if ((r = sys_arg_ulong(2, &mode)) < 0)
    goto out2;

  if ((r = fs_open(path, oflag, mode, &file)) < 0)
    goto out2;

  r = fd_alloc(process_current(), file, 0);

  file_put(file);
out2:
  k_free(path);
out1:
  return r;
}

int32_t
sys_link(void)
{
  char *path1, *path2;
  int r;

  if ((r = sys_arg_str(0, PATH_MAX, VM_READ, &path1)) < 0)
    goto out1;
  if ((r = sys_arg_str(1, PATH_MAX, VM_READ, &path2)) < 0)
    goto out2;

  r = fs_link(path1, path2);

  k_free(path2);
out2:
  k_free(path1);
out1:
  return r;
}

int32_t
sys_mknod(void)
{
  char *path;
  mode_t mode;
  dev_t dev;
  int r;

  if ((r = sys_arg_str(0, PATH_MAX, VM_READ, &path)) < 0)
    goto out1;
  if ((r = sys_arg_ulong(1, &mode)) < 0)
    goto out2;
  if ((r = sys_arg_short(2, &dev)) < 0)
    goto out2;
  
  r = fs_create(path, mode, dev, NULL);

out2:
  k_free(path);
out1:
  return r;
}

int32_t
sys_unlink(void)
{
  char *path;
  int r;

  if ((r = sys_arg_str(0, PATH_MAX, VM_READ, &path)) < 0)
    return r;

  r = fs_unlink(path);

  k_free(path);
  return r;
}

int32_t
sys_rmdir(void)
{
  char *path;
  int r;

  if ((r = sys_arg_str(0, PATH_MAX, VM_READ, &path)) < 0)
    return r;

  r = fs_rmdir(path);

  k_free(path);
  return r;
}

int32_t
sys_readlink(void)
{
  size_t buf_size;
  uintptr_t buf_va;
  int r;
  char *path, *buf;

  if ((r = sys_arg_str(0, PATH_MAX, VM_READ, &path)) < 0)
    goto out1;
  if ((r = sys_arg_uint(2, &buf_size)) < 0)
    goto out2;
  if ((r = sys_arg_va(1, &buf_va, buf_size, VM_WRITE, 0)) < 0)
    goto out2;

  if ((buf = (char *) k_malloc(NAME_MAX+1)) == NULL) {
    r = -ENOMEM;
    goto out2;
  }
  
  if ((r = fs_readlink(path, buf, NAME_MAX)) < 0)
    goto out3;

  buf_size = MIN(buf_size, (size_t) r);

  if ((r = sys_copy_out(buf, buf_va, buf_size)) < 0)
    goto out3;

  r = buf_size;

out3:
  k_free(buf);
out2:
  k_free(path);
out1:
  return r;
}

/*
 * ----------------------------------------------------------------------------
 * File descriptor system calls
 * ----------------------------------------------------------------------------
 */

int32_t
sys_getdents(void)
{
  size_t n;
  int fd;
  struct File *file;
  uintptr_t va;
  int r;

  if ((r = sys_arg_int(0, &fd)) < 0)
    return r;
  if ((r = sys_arg_uint(2, &n)) < 0)
    return r;
  if ((r = sys_arg_va(1, &va, n, VM_WRITE, 0)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  r = file_getdents(file, va, n);

  file_put(file);

  return r;
}

int32_t
sys_fchdir(void)
{
  struct File *file;
  int r, fd;

  if ((r = sys_arg_int(0, &fd)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  r = file_chdir(file);

  file_put(file);

  return r;
}

int32_t
sys_stat(void)
{
  struct File *file;
  uintptr_t buf_va;
  struct stat buf;
  int r, fd;

  if ((r = sys_arg_int(0, &fd)) < 0)
    goto out1;
  if ((r = sys_arg_va(1, &buf_va, sizeof buf, VM_WRITE, 0)) < 0)
    goto out1;

  if ((file = fd_lookup(process_current(), fd)) == NULL) {
    r = -EBADF;
    goto out2;
  }

  if ((r = file_stat(file, &buf)) < 0)
    goto out2;

  r = sys_copy_out(&buf, buf_va, sizeof buf);

out2:
  file_put(file);
out1:
  return r;
}

int32_t
sys_close(void)
{
  int r, fd;

  if ((r = sys_arg_int(0, &fd)) < 0)
    return r;

  return fd_close(process_current(), fd);
}

int32_t
sys_read(void)
{
  uintptr_t va;
  size_t n;
  struct File *file;
  int r, fd;

  if ((r = sys_arg_int(0, &fd)) < 0)
    return r;
  if ((r = sys_arg_uint(2, &n)) < 0)
    return r;
  if ((r = sys_arg_va(1, &va, n, VM_READ, 0)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  r = file_read(file, va, n);

  file_put(file);

  return r;
}

int32_t
sys_seek(void)
{
  struct File *file;
  off_t offset;
  int whence, r, fd;

  if ((r = sys_arg_int(0, &fd)) < 0)
    return r;
  if ((r = sys_arg_long(1, &offset)) < 0)
    return r;
  if ((r = sys_arg_int(2, &whence)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  r = file_seek(file, offset, whence);

  file_put(file);

  return r;
}

int32_t
sys_fcntl(void)
{
  struct File *file;
  int cmd, r, arg, fd;

  if ((r = sys_arg_int(0, &fd)) < 0)
    return r;
  if ((r = sys_arg_int(1, &cmd)) < 0)
    return r;
  if ((r = sys_arg_int(2, &arg)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  switch (cmd) {
  case F_DUPFD:
    r = fd_alloc(process_current(), file, arg);
    break;
  case F_GETFL:
    r = file_get_flags(file);
    break;
  case F_SETFL:
    r = file_set_flags(file, arg);
    break;
  case F_GETFD:
    r = fd_get_flags(process_current(), fd);
    break;
  case F_SETFD:
    r = fd_set_flags(process_current(), fd, arg);
    break;

  case F_GETOWN:
  case F_SETOWN:
  case F_GETLK:
  case F_SETLK:
  case F_SETLKW:
    cprintf("TODO: fcntl(%d)\n", cmd);
    r = -ENOSYS;
    break;

  default:
    r = -EINVAL;
    break;
  }

  file_put(file);
  return r;
}

int32_t
sys_write(void)
{
  uintptr_t va;
  size_t n;
  struct File *file;
  int r, fd;

  if ((r = sys_arg_int(0, &fd)) < 0)
    return r;
  if ((r = sys_arg_uint(2, &n)) < 0)
    return r;
  if ((r = sys_arg_va(1, &va, n, VM_WRITE, 0)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  r = file_write(file, va, n);

  file_put(file);

  return r;
}

int32_t
sys_fchmod(void)
{
  struct File *file;
  mode_t mode;
  int r, fd;

  if ((r = sys_arg_int(0, &fd)) < 0)
    return r;
  if ((r = sys_arg_ulong(1, &mode)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  r = file_chmod(file, mode);

  file_put(file);

  return r;
}

int32_t
sys_fchown(void)
{
  struct File *file;
  int r, fd;
  uid_t uid;
  gid_t gid;

  if ((r = sys_arg_int(0, &fd)) < 0)
    return r;
  if ((r = sys_arg_ushort(1, &uid)) < 0)
    return r;
  if ((r = sys_arg_ushort(2, &gid)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  r = file_chown(file, uid, gid);

  file_put(file);

  return r;
}

int32_t
sys_select(void)
{
  int r, fd, nfds;
  fd_set *readfds, *writefds, *errorfds;
  struct timeval *timeout;

  if ((r = sys_arg_int(0, &nfds)) < 0)
    return r;
  if ((r = sys_arg_va(1, (void *) &readfds, sizeof(fd_set), VM_READ, 1)) < 0)
    return r;
  if ((r = sys_arg_va(2, (void *) &writefds, sizeof(fd_set), VM_READ, 1)) < 0)
    return r;
  if ((r = sys_arg_va(3, (void *) &errorfds, sizeof(fd_set), VM_READ, 1)) < 0)
    return r;
  if ((r = sys_arg_va(4, (void *) &timeout, sizeof(struct timeval), VM_READ, 1)) < 0)
    return r;

  // TODO: writefds
  // TODO: errorfds
  // TODO: timeout

  if (readfds == NULL)
    return 0;
  
  r = 0;

  for (fd = 0; fd < FD_SETSIZE; fd++) {
    struct File *file;

    if (!FD_ISSET(fd, readfds))
      continue;

    if ((file = fd_lookup(process_current(), fd)) == NULL)
      return -EBADF;

    r += file_select(file);

    file_put(file);
  }

  return r;
}

int32_t
sys_ioctl(void)
{
  struct File *file;
  int r, request, fd, arg;

  if ((r = sys_arg_int(0, &fd)) < 0)
    return r;
  if ((r = sys_arg_int(1, &request)) < 0)
    return r;
  if ((r = sys_arg_int(2, &arg)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  r = file_ioctl(file, request, arg);
  
  file_put(file);

  return r;
}

int32_t
sys_ftruncate(void)
{
  struct File *file;
  int r, fd;
  off_t length;

  if ((r = sys_arg_int(0, &fd)) < 0)
    return r;
  if ((r = sys_arg_long(1, &length)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  r = file_truncate(file, length);

  file_put(file);

  return r;
}

int32_t
sys_fsync(void)
{
  struct File *file;
  int r, fd;

  if ((r = sys_arg_int(0, &fd)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  r = file_sync(file);

  file_put(file);

  return r;
}

/*
 * ----------------------------------------------------------------------------
 * Net system calls
 * ----------------------------------------------------------------------------
 */

int32_t
sys_socket(void)
{
  int r;
  int domain;
  int type;
  int protocol;
  struct File *file;

  if ((r = sys_arg_int(0, &domain)) < 0)
    return r;
  if ((r = sys_arg_int(1, &type)) < 0)
    return r;
  if ((r = sys_arg_int(2, &protocol)) < 0)
    return r;

  if ((r = net_socket(domain, type, protocol, &file)) != 0)
    return r;

  r = fd_alloc(process_current(), file, 0);

  file_put(file);
  return r;
}

int32_t
sys_bind(void)
{
  struct File *file;
  struct sockaddr *address;
  socklen_t address_len;
  int r, fd;

  if ((r = sys_arg_int(0, &fd)) < 0)
    return r;
  if ((r = sys_arg_va(1, (uintptr_t *) &address, sizeof(*address), VM_READ, 0)) < 0)
    return r;
  if ((r = sys_arg_ulong(2, &address_len)) < 0)
    return r;
  
  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  r = net_bind(file, address, address_len);

  file_put(file);

  return r;
}

int32_t
sys_connect(void)
{
  struct File *file;
  struct sockaddr *address;
  socklen_t address_len;
  int r, fd;

  if ((r = sys_arg_int(0, &fd)) < 0)
    return r;
  if ((r = sys_arg_va(1, (uintptr_t *) &address, sizeof(*address), VM_READ, 0)) < 0)
    return r;
  if ((r = sys_arg_ulong(2, &address_len)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  r = net_connect(file, address, address_len);

  file_put(file);

  return r;
}

int32_t
sys_listen(void)
{
  struct File *file;
  int backlog, r, fd;

  if ((r = sys_arg_int(0, &fd)) < 0)
    return r;
  if ((r = sys_arg_int(1, &backlog)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  r = net_listen(file, backlog);

  file_put(file);

  return r;
}

int32_t
sys_accept(void)
{
  struct File *sockf, *connf;
  struct sockaddr *address;
  socklen_t *address_len;
  int r, fd;

  if ((r = sys_arg_int(0, &fd)) < 0)
    goto out1;
  if ((r = sys_arg_va(1, (uintptr_t *) &address, sizeof(*address), VM_WRITE, 1)) < 0)
    goto out1;
  if ((r = sys_arg_va(2, (uintptr_t *) &address_len, sizeof(socklen_t), VM_WRITE, 1)) < 0)
    goto out1;

  if ((sockf = fd_lookup(process_current(), fd)) == NULL) {
    r = -EBADF;
    goto out1;
  }

  if ((r = net_accept(sockf, address, address_len, &connf)) < 0)
    goto out2;

  r = fd_alloc(process_current(), connf, 0);

  file_put(connf);
out2:
  file_put(sockf);
out1:
  return r;
}

/*
 * ----------------------------------------------------------------------------
 * Signal system calls
 * ----------------------------------------------------------------------------
 */

int32_t
sys_sigaction(void)
{
  int sig;
  uintptr_t stub;
  struct sigaction *act, *oact;
  int r;

  if ((r = sys_arg_int(0, &sig)) < 0)
    return r;
  if ((r = sys_arg_ptr(1, &stub, VM_READ | VM_EXEC, 1)) < 0)
    return r;
  if ((r = sys_arg_va(2, (uintptr_t *) &act, sizeof(*act), VM_READ, 1)) < 0)
    return r;
  if ((r = sys_arg_va(3, (uintptr_t *) &oact, sizeof(*oact), VM_WRITE, 1)) < 0)
    return r;

  return signal_action(sig, stub, act, oact);
}

int32_t
sys_sigreturn(void)
{
  return signal_return();
}

int32_t
sys_recvfrom(void)
{
  struct File *file;
  void *buffer;
  size_t length;
  int flags;
  struct sockaddr *address;
  socklen_t *address_len;
  int r, fd;

  if ((r = sys_arg_int(0, &fd)) < 0)
    return r;
  if ((r = sys_arg_uint(2, &length)) < 0)
    return r;
  if ((r = sys_arg_va(1, (uintptr_t *) &buffer, length, VM_WRITE, 0)) < 0)
    return r;
  if ((r = sys_arg_int(3, &flags)) < 0)
    return r;
  if ((r = sys_arg_va(4, (uintptr_t *) &address, sizeof(*address), VM_WRITE, 1)) < 0)
    return r;
  if ((r = sys_arg_va(5, (uintptr_t *) &address_len, sizeof(*address_len), VM_WRITE, 1)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  r = net_recvfrom(file, (uintptr_t) buffer, length, flags, address, address_len);

  file_put(file);

  return r;
}

int32_t
sys_sendto(void)
{
  struct File *file;
  void *message;
  size_t length;
  int flags;
  struct sockaddr *dest_addr;
  socklen_t dest_len;
  int r, fd;

  if ((r = sys_arg_int(0, &fd)) < 0)
    return r;
  if ((r = sys_arg_uint(2, &length)) < 0)
    return r;
  if ((r = sys_arg_va(1, (uintptr_t *) &message, length, VM_READ, 0)) < 0)
    return r;
  if ((r = sys_arg_int(3, &flags)) < 0)
    return r;
  if ((r = sys_arg_va(4, (uintptr_t *) &dest_addr, sizeof(*dest_addr), VM_READ, 1)) < 0)
    return r;
  if ((r = sys_arg_ulong(5, &dest_len)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  r = net_sendto(file, (uintptr_t) message, length, flags, dest_addr, dest_len);

  file_put(file);

  return r;
}

int32_t
sys_setsockopt(void)
{
  struct File *file;
  int level;
  int option_name;
  void *option_value;
  socklen_t option_len;
  int r, fd;

  if ((r = sys_arg_int(0, &fd)) < 0)
    return r;
  if ((r = sys_arg_int(1, &level)) < 0)
    return r;
  if ((r = sys_arg_int(2, &option_name)) < 0)
    return r;
  if ((r = sys_arg_ulong(4, &option_len)) < 0)
    return r;
  if ((r = sys_arg_va(3, (uintptr_t *) &option_value, option_len, VM_READ, 0)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  r = net_setsockopt(file, level, option_name, option_value, option_len);

  file_put(file);

  return r;
}

int32_t
sys_getpgid(void)
{
  pid_t pid;
  int r;
  
  if ((r = sys_arg_int(0, &pid)) < 0)
    return r;

  return process_get_gid(pid);
}

int32_t
sys_setpgid(void)
{
  pid_t pid, pgid;
  int r;
  
  if ((r = sys_arg_int(0, &pid)) < 0)
    return r;
  if ((r = sys_arg_int(1, &pgid)) < 0)
    return r;

  return process_set_gid(pid, pgid);
}

int32_t
sys_access(void)
{
  char *path;
  int r, amode;

  if ((r = sys_arg_str(0, PATH_MAX, VM_READ, &path)) < 0)
    goto out1;
  if ((r = sys_arg_int(1, &amode)) < 0)
    goto out2;
  
  r = fs_access(path, amode);

out2:
  k_free(path);
out1:
  return r;
}

/*
 * ----------------------------------------------------------------------------
 * Signal system calls
 * ----------------------------------------------------------------------------
 */

int32_t
sys_sigpending(void)
{
  sigset_t *set;
  int r;

  if ((r = sys_arg_va(0, (uintptr_t *) &set, sizeof(*set), VM_WRITE, 0)) < 0)
    return r;

  return signal_pending(set);
}

int32_t
sys_sigprocmask(void)
{
  sigset_t *set, *oset;
  int how, r;

  if ((r = sys_arg_int(0, &how)) < 0)
    return r;
  if ((r = sys_arg_va(1, (uintptr_t *) &set, sizeof(*set), VM_READ, 1)) < 0)
    return r;
  if ((r = sys_arg_va(2, (uintptr_t *) &oset, sizeof(*oset), VM_WRITE, 1)) < 0)
    return r;

  return signal_mask(how, set, oset);
}

int32_t
sys_sigsuspend(void)
{
  sigset_t *mask;
  int r;

  

  if ((r = sys_arg_va(0, (uintptr_t *) &mask, sizeof(*mask), VM_READ, 1)) < 0)
    return r;

  return signal_suspend(mask);
}

int32_t
sys_kill(void)
{
  pid_t pid;
  int sig, r;

  if ((r = sys_arg_int(0, &pid)) < 0)
    return r;
  if ((r = sys_arg_int(1, &sig)) < 0)
    return r;

  return signal_generate(pid, sig, 0);
}

/*
 * ----------------------------------------------------------------------------
 * Miscellaneous system calls
 * ----------------------------------------------------------------------------
 */

int32_t
sys_clock_time(void)
{
  clockid_t clock_id;
  uintptr_t prev;
  int r;
  
  if ((r = sys_arg_ulong(0, &clock_id)) < 0)
    return r;
  if ((r = sys_arg_va(1, &prev, sizeof(struct timespec), VM_WRITE, 1)) < 0)
    return r;

  if (clock_id != CLOCK_REALTIME)
    return -EINVAL;

  if (prev) {
    struct timespec prev_value;
    prev_value.tv_sec  = rtc_get_time();
    prev_value.tv_nsec = 0;

    if ((r = vm_copy_out(process_current()->vm->pgtab, &prev_value,
                         prev, sizeof prev_value)) < 0)
      return r;
  }

  return 0;
}

int32_t
sys_sbrk(void)
{
  panic("deprecated!\n");
  return -ENOSYS;
}

int32_t
sys_uname(void)
{
  extern struct utsname utsname;  // defined in main.c

  uintptr_t va;
  int r;

  if ((r = sys_arg_va(0, &va, sizeof utsname, VM_WRITE, 0)) < 0)
    return r;

  if ((r = vm_copy_out(process_current()->vm->pgtab, &utsname, va, sizeof utsname)) < 0)
    return r;

  return 0;
}

int32_t
sys_test(void)
{
  int i, r;

  for (i = 0; i < 6; i++) {
    int arg;

    if ((r = sys_arg_int(i, &arg)) < 0)
      return r;
    cprintf("[%d]: %d\n", i, arg);
  }
  return 0;
}

int32_t
sys_mmap(void)
{
  uintptr_t addr;
  size_t n;
  int prot;
  int r;

  if ((r = sys_arg_uint(0, &addr)) < 0)
    return r;
  if ((r = sys_arg_uint(1, &n)) < 0)
    return r;
  if ((r = sys_arg_int(2, &prot)) < 0)
    return r;

  return (int32_t) vmspace_map(process_current()->vm, addr, n, prot | VM_USER);
}

int32_t
sys_pipe(void)
{
  struct Process *my_process = process_current();
  uintptr_t fd_va;
  struct File *read_file, *write_file;
  int fd[2];
  int r;

  if ((r = sys_arg_va(0, &fd_va, sizeof fd, VM_WRITE, 0)) < 0)
    goto out1;

  if ((r = pipe_open(&read_file, &write_file)) < 0)
    goto out1;

  if ((fd[0] = r = fd_alloc(my_process, read_file, 0)) < 0)
    goto out2;
  if ((fd[1] = r = fd_alloc(my_process, write_file, 0)) < 0)
    goto out2;

  if ((r = vm_copy_out(my_process->vm->pgtab, fd, fd_va, sizeof fd)) < 0)
    goto out2;

  r = 0;

out2:
  file_put(read_file);
  file_put(write_file);
out1:
  return r;
}
