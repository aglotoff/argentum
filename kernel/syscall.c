#include <kernel/assert.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/stat.h>
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

#include <lwip/sockets.h>

static int     sys_get_num(void);
static int32_t sys_get_arg(int);

static int32_t (*syscalls[])(void) = {
  [__SYS_FORK]       = sys_fork,
  [__SYS_EXEC]       = sys_exec,
  [__SYS_WAIT]       = sys_wait,
  [__SYS_EXIT]       = sys_exit,
  [__SYS_GETPID]     = sys_getpid,
  [__SYS_GETPPID]    = sys_getppid,
  [__SYS_GETDENTS]   = sys_getdents,
  [__SYS_CHDIR]      = sys_chdir,
  [__SYS_FCHDIR]     = sys_fchdir,
  [__SYS_OPEN]       = sys_open,
  [__SYS_FCNTL]      = sys_fcntl,
  [__SYS_SEEK]       = sys_seek,
  [__SYS_UMASK]      = sys_umask,
  [__SYS_MKNOD]      = sys_mknod,
  [__SYS_LINK]       = sys_link,
  [__SYS_UNLINK]     = sys_unlink,
  [__SYS_RMDIR]      = sys_rmdir,
  [__SYS_STAT]       = sys_stat,
  [__SYS_CLOSE]      = sys_close,
  [__SYS_READ]       = sys_read,
  [__SYS_WRITE]      = sys_write,
  [__SYS_SBRK]       = sys_sbrk,
  [__SYS_UNAME]      = sys_uname,
  [__SYS_CHMOD]      = sys_chmod,
  [__SYS_CLOCK_TIME] = sys_clock_time,
  [__SYS_SOCKET]     = sys_socket,
  [__SYS_BIND]       = sys_bind,
  [__SYS_LISTEN]     = sys_listen,
  [__SYS_ACCEPT]     = sys_accept,
  [__SYS_CONNECT]    = sys_connect,
  [__SYS_TEST]       = sys_test,
  [__SYS_FCHMOD]     = sys_fchmod,
  [__SYS_SIGACTION]  = sys_sigaction,
  [__SYS_SIGRETURN]  = sys_sigreturn,
  [__SYS_NANOSLEEP]  = sys_nanosleep,
  [__SYS_RECVFROM]   = sys_recvfrom,
  [__SYS_SENDTO]     = sys_sendto,
  [__SYS_SETSOCKOPT] = sys_setsockopt,
  [__SYS_GETUID]     = sys_getuid,
  [__SYS_GETEUID]    = sys_geteuid,
  [__SYS_GETGID]     = sys_getgid,
  [__SYS_GETEGID]    = sys_getegid,
  [__SYS_GETPGID]    = sys_getpgid,
  [__SYS_SETPGID]    = sys_setpgid,
  [__SYS_ACCESS]     = sys_access,
  [__SYS_PIPE]       = sys_pipe,
  [__SYS_IOCTL]      = sys_ioctl,
  [__SYS_MMAP]       = sys_mmap,
};

int32_t
sys_dispatch(void)
{
  int num;

  if ((num = sys_get_num()) < 0)
    return num;

  if ((num < (int) ARRAY_SIZE(syscalls)) && syscalls[num]) {
    int r = syscalls[num]();
    // if (r < 0 || num == __SYS_FCNTL) 
    //   cprintf("syscall(%d) -> %d\n", num, r);
    return r;
  }

  cprintf("Unknown system call %d\n", num);
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
  struct Process *current = process_current();

  int *pc = (int *) (current->thread->tf->pc - 4);
  int r;

  if ((r = vm_space_check_buf(current->vm, pc, sizeof(int), PROT_READ)) < 0)
    return r;

  return *pc & 0xFFFFFF;
}

// Get the n-th argument from the current process' trap frame.
static int32_t
sys_get_arg(int n)
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
    if (n < 0)
      panic("Invalid argument number: %d", n);

    // TODO: grab additional parameters from the user stack.
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
static int
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
static int
sys_arg_short(int n, short *ip)
{
  *ip = (short) sys_get_arg(n);
  return 0;
}

static int
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
static int32_t
sys_arg_buf(int n, void **pp, size_t len, int perm, int can_be_null)
{ 
  void *ptr = (void *) sys_get_arg(n);
  int r;

  if (ptr == NULL) {
    if (can_be_null) {
      *pp = NULL;
      return 0;
    }
    return -EFAULT;
  }

  if ((r = vm_space_check_buf(process_current()->vm, ptr, len, perm)) < 0)
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
static int32_t
sys_arg_str(int n, const char **strp, int perm)
{
  char *str = (char *) sys_get_arg(n);
  int r;

  if ((r = vm_space_check_str(process_current()->vm, str, perm)) < 0)
    return r;

  *strp = str;

  return 0;
}

static int
sys_arg_args(int n, char ***store)
{
  struct Process *current = process_current();
  char **args;
  int i;

  args = (char **) sys_get_arg(n);
  
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
 * System Call Implementations
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
  const char *path;
  char **argv, **envp;
  int r;

  if ((r = sys_arg_str(0, &path, PROT_READ)) < 0)
    return r;
  if ((r = sys_arg_args(1, &argv)) < 0)
    return r;
  if ((r = sys_arg_args(2, &envp)) < 0)
    return r;

  return process_exec(path, argv, envp);
}

int32_t
sys_wait(void)
{
  pid_t pid;
  int *stat_loc;
  int r;
  
  if ((r = sys_arg_int(0, &pid)) < 0)
    return r;

  if ((r = sys_arg_buf(1, (void **) &stat_loc, sizeof(int), PROT_WRITE, 1)) < 0)
    return r;

  return process_wait(pid, (uintptr_t) stat_loc, 0);
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
sys_clock_time(void)
{
  clockid_t clock_id;
  struct timespec *prev;
  int r;
  
  if ((r = sys_arg_long(0, (long *) &clock_id)) < 0)
    return r;
  
  if ((r = sys_arg_buf(1, (void **) &prev, sizeof(*prev), PROT_WRITE, 1)) < 0)
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
sys_getdents(void)
{
  void *buf;
  size_t n;
  int fd;
  struct File *file;
  int r;

  if ((r = sys_arg_int(0, (int *) &fd)) < 0)
    return r;
  if ((r = sys_arg_int(2, (int *) &n)) < 0)
    return r;
  if ((r = sys_arg_buf(1, &buf, n, PROT_READ, 0)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  return file_getdents(file, buf, n);
}

int32_t
sys_chdir(void)
{
  const char *path;
  int r;

  if ((r = sys_arg_str(0, &path, PROT_READ)) < 0)
    return r;
  
  return fs_chdir(path);
}

int32_t
sys_chmod(void)
{
  const char *path;
  struct Inode *inode;
  mode_t mode;
  int r;

  if ((r = sys_arg_str(0, &path, PROT_READ)) < 0)
    return r;
  if ((r = sys_arg_short(1, (short *) &mode)) < 0)
    return r;

  if ((r = fs_name_lookup(path, 0, &inode)) < 0)
    return r;

  fs_inode_lock(inode);
  r = fs_inode_chmod(inode, mode);
  fs_inode_unlock_put(inode);

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

  return file_chdir(file);
}

int32_t
sys_open(void)
{
  struct File *file;
  const char *path;
  int oflag, r;
  mode_t mode;

  if ((r = sys_arg_str(0, &path, PROT_READ)) < 0)
    return r;
  if ((r = sys_arg_int(1, &oflag)) < 0)
    return r;
  if ((r = sys_arg_short(2, (short *) &mode)) < 0)
    return r;

  if ((r = file_open(path, oflag, mode, &file)) < 0)
    return r;

  if ((r = fd_alloc(process_current(), file, 0)) < 0)
    file_close(file);

  return r;
}

int32_t
sys_umask(void)
{
  struct Process *proc = process_current();
  mode_t cmask;
  int r;

  if ((r = sys_arg_short(0, (short *) &cmask)) < 0)
    return r;

  r = proc->cmask & (S_IRWXU | S_IRWXG | S_IRWXO);
  proc->cmask = cmask;

  return r;
}

int32_t
sys_link(void)
{
  const char *path1, *path2;
  int r;

  if ((r = sys_arg_str(0, &path1, PROT_READ)) < 0)
    return r;
  if ((r = sys_arg_str(1, &path2, PROT_READ)) < 0)
    return r;

  return fs_link((char *) path1, (char *) path2);
}

int32_t
sys_mknod(void)
{
  const char *path;
  mode_t mode;
  dev_t dev;
  int r;

  if ((r = sys_arg_str(0, &path, PROT_READ)) < 0)
    return r;
  if ((r = sys_arg_short(1, (short *) &mode)) < 0)
    return r;
  if ((r = sys_arg_short(2, &dev)) < 0)
    return r;

  return fs_create(path, mode, dev, NULL);
}

int32_t
sys_unlink(void)
{
  const char *path;
  int r;

  if ((r = sys_arg_str(0, &path, PROT_READ)) < 0)
    return r;

  return fs_unlink(path);
}

int32_t
sys_rmdir(void)
{
  const char *path;
  int r;

  if ((r = sys_arg_str(0, &path, PROT_READ)) < 0)
    return r;

  return fs_rmdir(path);
}

int32_t
sys_stat(void)
{
  struct File *file;
  struct stat *buf;
  int r, fd;

  if ((r = sys_arg_int(0, &fd)) < 0)
    return r;
  if ((r = sys_arg_buf(1, (void **) &buf, sizeof(*buf), PROT_WRITE, 0)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  return file_stat(file, buf);
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
  if ((r = sys_arg_int(2, (int *) &n)) < 0)
    return r;
  if ((r = sys_arg_buf(1, &buf, n, PROT_READ, 0)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  return file_read(file, buf, n);
}

int32_t
sys_seek(void)
{
  struct File *file;
  off_t offset;
  int whence, r, fd;

  if ((r = sys_arg_int(0, &fd)) < 0)
    return r;
  if ((r = sys_arg_long(1, (long *) &offset)) < 0)
    return r;
  if ((r = sys_arg_int(2, (int *) &whence)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  return file_seek(file, offset, whence);
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
    if ((r = fd_alloc(process_current(), file, arg)) >= 0)
      file_dup(file);
    return r;
  case F_GETFL:
    return file_get_flags(file);
  case F_SETFL:
    return file_set_flags(file, arg);
  case F_GETFD:
    return fd_get_flags(process_current(), fd);
  case F_SETFD:
    return fd_set_flags(process_current(), fd, arg);

  case F_GETOWN:
  case F_SETOWN:
  case F_GETLK:
  case F_SETLK:
  case F_SETLKW:
    cprintf("TODO: fcntl(%d)\n", cmd);
    return -ENOSYS;

  default:
    return -EINVAL;
  }
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
  if ((r = sys_arg_int(2, (int *) &n)) < 0)
    return r;
  if ((r = sys_arg_buf(1, &buf, n, PROT_WRITE, 0)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  return file_write(file, buf, n);
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

  if ((r = sys_arg_buf(0, (void **) &name, sizeof(*name), PROT_WRITE, 0)) < 0)
    return r;
  
  memcpy(name, &utsname, sizeof (*name));

  return 0;
}

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
    file_close(file);

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
  if ((r = sys_arg_buf(1, (void **) &address, sizeof(*address), PROT_READ, 0)) < 0)
    return r;
  if ((r = sys_arg_long(2, (long *) &address_len)) < 0)
    return r;
  
  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  if (file->type != FD_SOCKET)
    return -EBADF;

  return net_bind(file->socket, address, address_len);
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
  if ((r = sys_arg_buf(1, (void **) &address, sizeof(*address), PROT_READ, 0)) < 0)
    return r;
  if ((r = sys_arg_long(2, (long *) &address_len)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  if (file->type != FD_SOCKET)
    return -EBADF;

  return net_connect(file->socket, address, address_len);
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

  if (file->type != FD_SOCKET)
    return -EBADF;

  return net_listen(file->socket, backlog);
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
  if ((r = sys_arg_buf(1, (void **) &address, sizeof(*address), PROT_WRITE, 1)) < 0)
    return r;
  if ((r = sys_arg_buf(2, (void **) &address_len, sizeof(socklen_t), PROT_WRITE, 1)) < 0)
    return r;

  if ((sockf = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  if (sockf->type != FD_SOCKET)
    return -EBADF;

  if ((r = net_accept(sockf->socket, address, address_len, &connf)) < 0)
    return r;

  if ((r = fd_alloc(process_current(), connf, 0)) < 0)
    file_close(connf);

  return r;
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
sys_fchmod(void)
{
  struct File *file;
  mode_t mode;
  int r, fd;

  if ((r = sys_arg_int(0, &fd)) < 0)
    return r;
  if ((r = sys_arg_short(1, (short *) &mode)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  return file_chmod(file, mode);
}

int32_t
sys_sigaction(void)
{
  int sig;
  uintptr_t stub;
  struct sigaction *act, *oact;
  int r;

  if ((r = sys_arg_int(0, &sig)) < 0)
    return r;
  if ((r = sys_arg_long(1, (long *) &stub)) < 0)
    return r;
  if ((r = sys_arg_buf(2, (void **) &act, sizeof(*act), PROT_READ, 1)) < 0)
    return r;
  if ((r = sys_arg_buf(3, (void **) &oact, sizeof(*oact), PROT_WRITE, 1)) < 0)
    return r;

  return process_signal_action(sig, stub, act, oact);
}

int32_t
sys_sigreturn(void)
{
  return process_signal_return();
}

int32_t
sys_nanosleep(void)
{
  struct timespec *rqtp, *rmtp;
  int r;

  if ((r = sys_arg_buf(0, (void **) &rqtp, sizeof(*rqtp), PROT_READ, 0)) < 0)
    return r;
  if ((r = sys_arg_buf(1, (void **) &rmtp, sizeof(*rmtp), PROT_WRITE, 1)) < 0)
    return r;

  return process_nanosleep(rqtp, rmtp);
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
  if ((r = sys_arg_int(2, (int *) &length)) < 0)
    return r;
  if ((r = sys_arg_buf(1, &buffer, length, PROT_WRITE, 0)) < 0)
    return r;
  if ((r = sys_arg_int(3, (int *) &flags)) < 0)
    return r;
  if ((r = sys_arg_buf(4, (void **) &address, sizeof(*address), PROT_WRITE, 1)) < 0)
    return r;
  if ((r = sys_arg_buf(5, (void **) &address_len, sizeof(*address_len), PROT_WRITE, 1)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  if (file->type != FD_SOCKET)
    return -EBADF;

  return net_recvfrom(file->socket, buffer, length, flags, address, address_len);
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
  if ((r = sys_arg_int(2, (int *) &length)) < 0)
    return r;
  if ((r = sys_arg_buf(1, &message, length, PROT_READ, 0)) < 0)
    return r;
  if ((r = sys_arg_int(3, (int *) &flags)) < 0)
    return r;
  if ((r = sys_arg_buf(4, (void **) &dest_addr, sizeof(*dest_addr), PROT_READ, 1)) < 0)
    return r;
  if ((r = sys_arg_int(5, (int *) &dest_len)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  if (file->type != FD_SOCKET)
    return -EBADF;

  return net_sendto(file->socket, message, length, flags, dest_addr, dest_len);
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
  if ((r = sys_arg_int(1, (int *) &level)) < 0)
    return r;
  if ((r = sys_arg_int(2, (int *) &option_name)) < 0)
    return r;
  if ((r = sys_arg_int(4, (int *) &option_len)) < 0)
    return r;
  if ((r = sys_arg_buf(3, &option_value, option_len, PROT_READ, 0)) < 0)
    return r;

  if ((file = fd_lookup(process_current(), fd)) == NULL)
    return -EBADF;

  if (file->type != FD_SOCKET)
    return -EBADF;

  return net_setsockopt(file->socket, level, option_name, option_value, option_len);
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
  const char *path;
  int r, amode;

  if ((r = sys_arg_str(0, &path, PROT_READ)) < 0)
    return r;
  if ((r = sys_arg_int(1, &amode)) < 0)
    return r;
  
  return fs_access(path, amode);
}

int32_t
sys_pipe(void)
{
  int r;
  int *fildes;
  struct File *read, *write;

  if ((r = sys_arg_buf(0, (void **) &fildes, sizeof(int)*2, PROT_WRITE, 0)) < 0)
    return r;

  if ((r = pipe_alloc(&read, &write)) < 0)
    return r;

  if ((fildes[0] = r = fd_alloc(process_current(), read, 0)) < 0) {
    file_close(read);
    file_close(write);
    return r;
  }

  if ((fildes[1] = r = fd_alloc(process_current(), write, 0)) < 0) {
    file_close(read);
    file_close(write);
    return r;
  }

  return 0;
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
  //cprintf("ioctl(%x, %d) -> %d\n", request, arg, r);
  return r;
}

int32_t
sys_mmap(void)
{
  uintptr_t addr;
  size_t n;
  int prot;
  int r;

  if ((r = sys_arg_long(0, (long *) &addr)) < 0)
    return r;
  if ((r = sys_arg_long(1, (long *) &n)) < 0)
    return r;
  if ((r = sys_arg_int(2, &prot)) < 0)
    return r;

  // vm_print_areas(process_current()->vm);

  uintptr_t aa = vmspace_map(process_current()->vm, addr, n, prot | _PROT_USER);
  // cprintf("mmap(%p, %p, %d) -> %p\n", addr, n, prot, aa);

  // vm_print_areas(process_current()->vm);

  return (int32_t) aa;
}
