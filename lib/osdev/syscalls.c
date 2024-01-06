#include <syscall.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/times.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <dirent.h>
#include <limits.h>
#include <unistd.h>
#include <signal.h>

int
fcntl(int fildes, int cmd, ...)
{
  int arg;
  va_list ap;

  switch (cmd) {
  case F_SETFD:
    va_start(ap, cmd);
    arg = va_arg(ap, int);
    va_end(ap);
    break;
  default:
    arg = 0;
    break;
  }

  return __syscall(__SYS_FCNTL, fildes, cmd, arg);
}


ssize_t
getdents(int fd, void *buf, size_t n)
{
  return __syscall(__SYS_GETDENTS, fd, (uint32_t) buf, n);
}


void
_exit(int status)
{
  __syscall(__SYS_EXIT, status, 0, 0);
}


int
_kill(pid_t pid, int sig)
{
  return -1;
}

void *
_sbrk(ptrdiff_t increment)
{
  return (void *) __syscall(__SYS_SBRK, increment, 0, 0);
}

int
_close(int fildes)
{
  return __syscall(__SYS_CLOSE, fildes, 0, 0);
}

int
close(int fildes)
{
  return _close(fildes);
}

ssize_t
_write(int fildes, const void *buf, size_t n)
{
  return __syscall(__SYS_WRITE, fildes, (uint32_t) buf, n);
}

ssize_t
write(int fildes, const void *buf, size_t n)
{
  return _write(fildes, buf, n);
}

ssize_t
_read(int fildes, void *buf, size_t n)
{
  return __syscall(__SYS_READ, fildes, (uint32_t) buf, n);
}

ssize_t
read(int fildes, void *buf, size_t n)
{
  return _read(fildes, buf, n);
}

off_t
_lseek(int fildes, off_t offset, int whence)
{
  return __syscall(__SYS_SEEK, fildes, offset, whence);
}

off_t
lseek(int fildes, off_t offset, int whence)
{
  return _lseek(fildes, offset, whence);
}

int
_fstat(int fildes, struct stat *buf)
{
  return __syscall(__SYS_STAT, fildes, (uintptr_t) buf, 0);
}

int
fstat(int fildes, struct stat *buf)
{
  return _fstat(fildes, buf);
}

int 
_fsync(int fd)
{
  (void) fd;
  return 0;
}

int
fsync(int fd)
{
  return _fsync(fd);
}

pid_t
_getpid(void)
{
  return __syscall(__SYS_GETPID, 0, 0, 0);
}


// int
// _isatty(int file)
// {
//   return 1;
// }

/**
 * Get the name of the current system.
 * 
 * @param name Pointer to the structure to store the system information.
 * 
 * @returns 0 on success, -1 otherwise.
 */
int
uname(struct utsname *name)
{
  return __syscall(__SYS_UNAME, (uintptr_t) name, 0, 0);
}

int
mknod(const char *path, mode_t mode, dev_t dev)
{
  return __syscall(__SYS_MKNOD, (uintptr_t) path, mode, dev);
}

int
_execve(const char *path, char *const argv[], char *const envp[])
{
  return __syscall(__SYS_EXEC, (uint32_t) path, (uint32_t) argv,
                   (uint32_t) envp);
}


int
chdir(const char *path)
{
  return __syscall(__SYS_CHDIR, (uint32_t) path, 0, 0);
}

pid_t
fork(void)
{
  return __syscall(__SYS_FORK, 0, 0, 0);
}

pid_t
wait(int *stat_loc)
{
  return __syscall(__SYS_WAIT, -1, (uint32_t) stat_loc, 0);
}

int
open(const char *path, int flags, ...)
{
  int mode = 0;

  if (flags & O_CREAT) {
    va_list ap;

    va_start(ap, flags);
    mode = va_arg(ap, int);
    va_end(ap);

    // TODO: umask
  }

  return __syscall(__SYS_OPEN, (uint32_t) path, flags, mode);
}

int
mkdir(const char *path, mode_t mode)
{
  return __syscall(__SYS_MKNOD, (uintptr_t) path, S_IFDIR | mode, 0);
}

mode_t
umask(mode_t cmask)
{
  return __syscall(__SYS_UMASK, cmask, 0, 0);
}


char dbuf[10240];
char name[PATH_MAX];

struct Entry {
  struct Entry *next;
  char *name;
};

static void
free_entries(struct Entry *e)
{
  while (e != NULL) {
    struct Entry *prev;
    
    prev = e;
    e = e->next;
  
    if (prev->name)
      free(prev->name);
    free(prev);
  }
}

char
*getcwd(char *buf, size_t size)
{
  struct stat st;
  dev_t curr_dev;
  ino_t curr_ino;
  int fd;
  struct Entry *path;
  char *s;

  if (size < 2) {
    errno = (size == 0) ? EINVAL : ERANGE;
    return NULL;
  }

  strcpy(name, ".");
  if (stat(name, &st) != 0)
    return NULL;

  curr_dev = st.st_dev;
  curr_ino = st.st_ino;
  path = NULL;

  for (;;) {
    dev_t parent_dev;
    ino_t parent_ino;
    ssize_t nread;
    struct Entry *next;

    strcat(name, "/..");
    if ((fd = open(name, O_RDONLY)) < 0)
      return NULL;

    if (fstat(fd, &st) != 0) {
      close(fd);
      return NULL;
    }

    parent_dev = st.st_dev;
    parent_ino = st.st_ino;

    if ((curr_dev == parent_dev) && (curr_ino == parent_ino)) {
      close(fd);
      break;
    }

    next = NULL;
    while ((next == NULL) && (nread = getdents(fd, dbuf, sizeof(dbuf))) != 0) {
      char *p;

      if (nread < 0) {
        close(fd);
        return NULL;
      }

      p = dbuf;
      while (p < &dbuf[nread]) {
        struct dirent *dp = (struct dirent *) p;

        if (dp->d_ino == curr_ino) {
          if (((next = malloc(sizeof(struct Entry))) == NULL) ||
              ((next->name = malloc(strlen(dp->d_name) + 1)) == NULL)) {

            if (next != NULL)
              free(next);
            free_entries(path);

            close(fd);
            errno = ENOMEM;
            return NULL;
          }

          strcpy(next->name, dp->d_name);

          next->next = path;
          path = next;

          break;
        }

        p += dp->d_reclen;
      }
    }

    close(fd);

    if (next == NULL) {
      free_entries(path);
      errno = EACCES;
      return NULL;
    }

    curr_dev = parent_dev;
    curr_ino = parent_ino;
  }

  s = buf;

  if (path == NULL) {
    strcpy(s, "/");
    return buf;
  }

  while (path != NULL) {
    if ((strlen(path->name) + 1) >= size) {
      errno = ERANGE;
      free_entries(path);
      return NULL;
    }

    size -= strlen(path->name) + 1;

    s += snprintf(s, size, "/%s", path->name);
    path = path->next;
  }

  free_entries(path);

  return buf;
}

pid_t
waitpid(pid_t pid, int *stat_loc, int options)
{
  return __syscall(__SYS_WAIT, pid, (uint32_t) stat_loc, options);
}

int
chmod(const char *path, mode_t mode)
{
  return __syscall(__SYS_CHMOD, (uintptr_t) path, mode, 0);
}

int
clock_gettime(clockid_t clock_id, struct timespec *tp)
{
  return __syscall(__SYS_CLOCK_TIME, clock_id, (uintptr_t) tp, 0);
}

int
_gettimeofday (struct timeval *tp, void *tzp)
{
  struct timespec t;
  int ret;

  (void) tzp;

  // TODO: replace by CLOCK_MONOTONIC
  ret = clock_gettime(CLOCK_REALTIME, &t);

  if (ret == 0) {
    tp->tv_sec  = t.tv_sec;
    tp->tv_usec = t.tv_nsec / 1000;
  }

  return ret;
}

int
gettimeofday (struct timeval *tp, void *tzp)
{
  return _gettimeofday(tp, tzp);
}

int
rmdir(const char *path)
{
  return __syscall(__SYS_RMDIR, (uint32_t) path, 0, 0);
}

int
_link(const char *path1, const char *path2)
{
  return __syscall(__SYS_LINK, (uint32_t) path1, (uint32_t) path2, 0);
}

int
link(const char *path1, const char *path2)
{
  return _link(path1, path2);
}

int
unlink(const char *path)
{
  return __syscall(__SYS_UNLINK, (uint32_t) path, 0, 0);
}

// int
// _fsync(int fildes)
// {
//   (void) fildes;

//   fprintf(stderr, "TODO: fsync");
//   abort();

//   return -1;
// }

int
pipe(int fildes[2])
{
  (void) fildes;

  fprintf(stderr, "TODO: pipe");
  abort();

  return -1;
}

int
sigaction(int sig, const struct sigaction *act, struct sigaction *oact)
{
  (void) sig;
  (void) act;
  (void) oact;

  // fprintf(stderr, "TODO: sigaction\n");

  return 0;
}

int
sigprocmask(int how, const sigset_t *set, sigset_t *oset)
{
  (void) how;
  (void) set;
  (void) oset;

  // fprintf(stderr, "TODO: sigprocmask\n");

  return 0;
}

int
utime(const char *path, const struct utimbuf *times)
{
  (void) path;
  (void) times;
  
  // fprintf(stderr, "TODO: utime\n");

  return 0;
}

int
dup2(int oldfd, int newfd)
{
  (void) oldfd;
  (void) newfd;

  fprintf(stderr, "TODO: dup2");
  abort();

  return -1;
}

int
access(const char *path, int amode)
{
  (void) path;
  (void) amode;

  fprintf(stderr, "TODO: access");
  abort();

  return -1;
}

int
getrlimit(int resource, struct rlimit *rlim)
{
  (void) resource;
  (void) rlim;

  fprintf(stderr, "TODO: getrlimit");
  abort();

  return -1;
}