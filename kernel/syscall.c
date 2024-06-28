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

  if ((r = vm_space_check_buf(current->vm, pc, sizeof(int), PROT_READ)) < 0)
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

  if ((r = vm_space_check_ptr(process_current()->vm, ptr, perm)) < 0)
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
sys_arg_buf(int n, uintptr_t *pp, size_t len, int perm, int can_be_null)
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

  if ((r = vm_space_check_buf(process_current()->vm, (void *) ptr, len, perm)) < 0)
    return r;

  *pp = ptr;

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

  if ((r = vm_check_str(pgtab, va, &len, perm)) < 0)
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

static int
sys_arg_args(int n, char ***store)
{
  struct Process *current = process_current();
  char **args;
  int i;

  args = (char **) sys_arch_get_arg(n);
  
  for (i = 0; ; i++) {
    int r;

    if ((r = vm_space_check_buf(current->vm, args + i, sizeof(args[i]),
                               PROT_READ)) < 0)
      return r;

    if (args[i] == NULL)
      break;
    
    if ((r = vm_space_check_str(current->vm, args[i], PROT_READ)) < 0)
      return r;
  }

  if (store)
    *store = args;

  return 0;
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
  int r;

  if ((r = sys_arg_str(0, PATH_MAX, PROT_READ, &path)) < 0)
    goto out1;
  if ((r = sys_arg_args(1, &argv)) < 0)
    goto out2;
  if ((r = sys_arg_args(2, &envp)) < 0)
    goto out2;

  r = process_exec(path, argv, envp);

out2:
  k_free(path);
out1:
  return r;
}

int32_t
sys_wait(void)
{
  pid_t pid;
  uintptr_t stat_loc;
  int r, stat, options;
  
  if ((r = sys_arg_int(0, &pid)) < 0)
    return r;
  if ((r = sys_arg_buf(1, &stat_loc, sizeof(int), PROT_WRITE, 1)) < 0)
    return r;
  if ((r = sys_arg_int(2, &options)) < 0)
    return r;

  if ((r = process_wait(pid, &stat, options)) < 0)
    return r;

  if (!stat_loc)
    return r;

  pid = r;

  r = vm_copy_out(process_current()->vm->pgtab, &stat, stat_loc, sizeof(int));
  if (r < 0)
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
  uintptr_t times_addr;
  struct tms times;
  int r;

  if ((r = sys_arg_buf(0, &times_addr, sizeof times, PROT_WRITE, 0)) < 0)
    return r;

  process_get_times(process_current(), &times);

  return vm_copy_out(process_current()->vm->pgtab, &times, times_addr,
                     sizeof times);
}

int32_t
sys_nanosleep(void)
{
  struct timespec *rqtp, *rmtp;
  int r;

  if ((r = sys_arg_buf(0, (uintptr_t *) &rqtp, sizeof(*rqtp), PROT_READ, 0)) < 0)
    return r;
  if ((r = sys_arg_buf(1, (uintptr_t *) &rmtp, sizeof(*rmtp), PROT_WRITE, 1)) < 0)
    return r;

  return process_nanosleep(rqtp, rmtp);
}

/*
 * ----------------------------------------------------------------------------
 * Path name system calls
 * ----------------------------------------------------------------------------
 */

int32_t
sys_chdir(void)
{
  char *path;
  int r;

  if ((r = sys_arg_str(0, PATH_MAX, PROT_READ, &path)) < 0)
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

  if ((r = sys_arg_str(0, PATH_MAX, PROT_READ, &path)) < 0)
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

  if ((r = sys_arg_str(0, PATH_MAX, PROT_READ, &path)) < 0)
    goto out1;
  if ((r = sys_arg_int(1, &oflag)) < 0)
    goto out2;
  if ((r = sys_arg_ulong(2, &mode)) < 0)
    goto out2;

  if ((r = fs_open(path, oflag, mode, &file)) < 0)
    goto out2;

  if ((r = fd_alloc(process_current(), file, 0)) < 0)
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

  if ((r = sys_arg_str(0, PATH_MAX, PROT_READ, &path1)) < 0)
    goto out1;
  if ((r = sys_arg_str(1, PATH_MAX, PROT_READ, &path2)) < 0)
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

  if ((r = sys_arg_str(0, PATH_MAX, PROT_READ, &path)) < 0)
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

  if ((r = sys_arg_str(0, PATH_MAX, PROT_READ, &path)) < 0)
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

  if ((r = sys_arg_str(0, PATH_MAX, PROT_READ, &path)) < 0)
    return r;

  r = fs_rmdir(path);

  k_free(path);
  return r;
}

int32_t
sys_readlink(void)
{
  void *buf;
  size_t n;
  int r;
  char *path;

  if ((r = sys_arg_str(0, PATH_MAX, PROT_READ, &path)) < 0)
    goto out1;
  if ((r = sys_arg_uint(2, &n)) < 0)
    goto out2;
  if ((r = sys_arg_buf(1, (uintptr_t *) &buf, n, PROT_WRITE, 0)) < 0)
    goto out2;
  
  r = fs_readlink(path, buf, n);

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
  void *buf;
  size_t n;
  int fd;
  struct File *file;
  int r;

  if ((r = sys_arg_int(0, &fd)) < 0)
    return r;
  if ((r = sys_arg_uint(2, &n)) < 0)
    return r;
  if ((r = sys_arg_buf(1, (uintptr_t *) &buf, n, PROT_WRITE, 0)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  r = file_getdents(file, buf, n);

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
  struct stat *buf;
  int r, fd;

  if ((r = sys_arg_int(0, &fd)) < 0)
    return r;
  if ((r = sys_arg_buf(1, (uintptr_t *) &buf, sizeof(*buf), PROT_WRITE, 0)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  r = file_stat(file, buf);

  file_put(file);

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
  void *buf;
  size_t n;
  struct File *file;
  int r, fd;

  if ((r = sys_arg_int(0, &fd)) < 0)
    return r;
  if ((r = sys_arg_uint(2, &n)) < 0)
    return r;
  if ((r = sys_arg_buf(1, (uintptr_t *) &buf, n, PROT_READ, 0)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  r = file_read(file, buf, n);

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
    return fd_alloc(process_current(), file, arg);
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
  void *buf;
  size_t n;
  struct File *file;
  int r, fd;

  if ((r = sys_arg_int(0, &fd)) < 0)
    return r;
  if ((r = sys_arg_uint(2, &n)) < 0)
    return r;
  if ((r = sys_arg_buf(1, (uintptr_t *) &buf, n, PROT_WRITE, 0)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  r = file_write(file, buf, n);

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
  if ((r = sys_arg_buf(1, (void *) &readfds, sizeof(fd_set), PROT_READ, 1)) < 0)
    return r;
  if ((r = sys_arg_buf(2, (void *) &writefds, sizeof(fd_set), PROT_READ, 1)) < 0)
    return r;
  if ((r = sys_arg_buf(3, (void *) &errorfds, sizeof(fd_set), PROT_READ, 1)) < 0)
    return r;
  if ((r = sys_arg_buf(4, (void *) &timeout, sizeof(struct timeval), PROT_READ, 1)) < 0)
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

  if ((r = fd_alloc(process_current(), file, 0)) < 0)
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
  if ((r = sys_arg_buf(1, (uintptr_t *) &address, sizeof(*address), PROT_READ, 0)) < 0)
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
  if ((r = sys_arg_buf(1, (uintptr_t *) &address, sizeof(*address), PROT_READ, 0)) < 0)
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
    return r;
  if ((r = sys_arg_buf(1, (uintptr_t *) &address, sizeof(*address), PROT_WRITE, 1)) < 0)
    return r;
  if ((r = sys_arg_buf(2, (uintptr_t *) &address_len, sizeof(socklen_t), PROT_WRITE, 1)) < 0)
    return r;

  if ((sockf = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  if ((r = net_accept(sockf, address, address_len, &connf)) < 0) {
    file_put(sockf);
    return r;
  }

  if ((r = fd_alloc(process_current(), connf, 0)) < 0)
    file_put(connf);

  file_put(sockf);

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
  if ((r = sys_arg_ptr(1, &stub, PROT_READ | PROT_EXEC, 1)) < 0)
    return r;
  if ((r = sys_arg_buf(2, (uintptr_t *) &act, sizeof(*act), PROT_READ, 1)) < 0)
    return r;
  if ((r = sys_arg_buf(3, (uintptr_t *) &oact, sizeof(*oact), PROT_WRITE, 1)) < 0)
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
  if ((r = sys_arg_buf(1, (uintptr_t *) &buffer, length, PROT_WRITE, 0)) < 0)
    return r;
  if ((r = sys_arg_int(3, &flags)) < 0)
    return r;
  if ((r = sys_arg_buf(4, (uintptr_t *) &address, sizeof(*address), PROT_WRITE, 1)) < 0)
    return r;
  if ((r = sys_arg_buf(5, (uintptr_t *) &address_len, sizeof(*address_len), PROT_WRITE, 1)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  r = net_recvfrom(file, buffer, length, flags, address, address_len);

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
  if ((r = sys_arg_buf(1, (uintptr_t *) &message, length, PROT_READ, 0)) < 0)
    return r;
  if ((r = sys_arg_int(3, &flags)) < 0)
    return r;
  if ((r = sys_arg_buf(4, (uintptr_t *) &dest_addr, sizeof(*dest_addr), PROT_READ, 1)) < 0)
    return r;
  if ((r = sys_arg_ulong(5, &dest_len)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  r = net_sendto(file, message, length, flags, dest_addr, dest_len);

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
  if ((r = sys_arg_buf(3, (uintptr_t *) &option_value, option_len, PROT_READ, 0)) < 0)
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

  if ((r = sys_arg_str(0, PATH_MAX, PROT_READ, &path)) < 0)
    goto out1;
  if ((r = sys_arg_int(1, &amode)) < 0)
    goto out2;
  
  r = fs_access(path, amode);

out2:
  k_free(path);
out1:
  return r;
}

int32_t
sys_pipe(void)
{
  int r;
  int *fildes;
  struct File *read, *write;

  if ((r = sys_arg_buf(0, (uintptr_t *) &fildes, sizeof(int)*2, PROT_WRITE, 0)) < 0)
    return r;

  if ((r = pipe_open(&read, &write)) < 0)
    return r;

  if ((fildes[0] = r = fd_alloc(process_current(), read, 0)) < 0) {
    file_put(read);
    file_put(write);
    return r;
  }

  if ((fildes[1] = r = fd_alloc(process_current(), write, 0)) < 0) {
    file_put(read);
    file_put(write);
    return r;
  }

  return 0;
}






int32_t
sys_sigpending(void)
{
  sigset_t *set;
  int r;

  if ((r = sys_arg_buf(0, (uintptr_t *) &set, sizeof(*set), PROT_WRITE, 0)) < 0)
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
  if ((r = sys_arg_buf(1, (uintptr_t *) &set, sizeof(*set), PROT_READ, 1)) < 0)
    return r;
  if ((r = sys_arg_buf(2, (uintptr_t *) &oset, sizeof(*oset), PROT_WRITE, 1)) < 0)
    return r;

  return signal_mask(how, set, oset);
}

int32_t
sys_sigsuspend(void)
{
  sigset_t *mask;
  int r;

  if ((r = sys_arg_buf(0, (uintptr_t *) &mask, sizeof(*mask), PROT_READ, 1)) < 0)
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
  struct timespec *prev;
  int r;
  
  if ((r = sys_arg_ulong(0, &clock_id)) < 0)
    return r;
  
  if ((r = sys_arg_buf(1, (uintptr_t *) &prev, sizeof(*prev), PROT_WRITE, 1)) < 0)
    return r;

  if (clock_id != CLOCK_REALTIME)
    return -EINVAL;

  if (prev) {
    struct timespec prev_value;
    prev_value.tv_sec  = rtc_get_time();
    prev_value.tv_nsec = 0;

    if ((r = vm_copy_out(process_current()->vm->pgtab, &prev_value,
                         (uintptr_t) prev, sizeof prev_value)) < 0)
      return r;
  }

  return 0;
}

int32_t
sys_sbrk(void)
{
  ptrdiff_t n;
  int r;

  if ((r = sys_arg_int(0, &n)) < 0)
    return r;

  panic("deprecated!\n");
  
  return (int32_t) process_grow(n);
}

int32_t
sys_uname(void)
{
  extern struct utsname utsname;  // defined in main.c

  struct utsname *name;
  int r;

  if ((r = sys_arg_buf(0, (uintptr_t *) &name, sizeof(*name), PROT_WRITE, 0)) < 0)
    return r;
  
  memcpy(name, &utsname, sizeof (*name));

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

  return (int32_t) vmspace_map(process_current()->vm, addr, n, prot | _PROT_USER);
}
