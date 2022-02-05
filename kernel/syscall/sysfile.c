#include <errno.h>
#include <stddef.h>
#include <sys/stat.h>

#include <kernel/drivers/console.h>
#include <kernel/fs/file.h>
#include <kernel/fs/fs.h>
#include <kernel/mm/vm.h>
#include <kernel/process.h>
#include <kernel/syscall.h>

int32_t
sys_getdents(void)
{
  void *buf;
  size_t n;
  struct File *f;
  int r;

  if ((r = sys_arg_fd(0, NULL, &f)) < 0)
    return r;

  if ((r = sys_arg_int(2, (int *) &n)) < 0)
    return r;

  if ((r = sys_arg_buf(1, &buf, n, VM_U)) < 0)
    return r;

  return file_getdents(f, buf, n);
}

int32_t
sys_chdir(void)
{
  const char *path;
  struct Inode *ip;
  int r;

  if ((r = sys_arg_str(0, &path, VM_U)) < 0)
    return r;
  
  if ((r = fs_name_lookup(path, &ip)) < 0)
    return r;
  
  fs_inode_lock(ip);

  if (!S_ISDIR(ip->mode)) {
    fs_inode_unlock(ip);
    fs_inode_put(ip);
    return -ENOTDIR;
  }

  fs_inode_unlock(ip);

  my_process()->cwd = ip;

  return 0;
}

static int
fd_alloc(struct File *f)
{
  struct Process *current = my_process();
  int i;

  for (i = 0; i < OPEN_MAX; i++)
    if (current->files[i] == NULL) {
      current->files[i] = f;
      return i;
    }
  return -ENFILE;
}

int32_t
sys_open(void)
{
  struct File *f;
  const char *path;
  int oflag, r;

  if ((r = sys_arg_str(0, &path, VM_U)) < 0)
    return r;
  if ((r = sys_arg_int(1, &oflag)) < 0)
    return r;

  if ((r = file_open(path, oflag, &f)) < 0)
    return r;

  if ((r = fd_alloc(f)) < 0)
    file_close(f);

  return r;
}

int32_t
sys_mkdir(void)
{
  const char *path;
  mode_t mode;
  int r;

  if ((r = sys_arg_str(0, &path, VM_U)) < 0)
    return r;

  if ((r = sys_arg_short(1, (short *) &mode)) < 0)
    return r;

  if (mode & ~(S_IRWXU | S_IRWXG | S_IRWXO))
    return -EINVAL;

  return fs_create(path, S_IFDIR | mode, 0, NULL);
}

int32_t
sys_mknod(void)
{
  const char *path;
  mode_t mode;
  dev_t dev;
  int r;

  if ((r = sys_arg_str(0, &path, VM_U)) < 0)
    return r;
  if ((r = sys_arg_short(1, (short *) &mode)) < 0)
    return r;
  if ((r = sys_arg_short(2, &dev)) < 0)
    return r;

  return fs_create(path, mode, dev, NULL);
}

int32_t
sys_stat(void)
{
  struct File *f;
  struct stat *buf;
  int r;

  if ((r = sys_arg_fd(0, NULL, &f)) < 0)
    return r;

  if ((r = sys_arg_buf(1, (void **) &buf, sizeof(*buf), VM_U | VM_W)) < 0)
    return r;

  return file_stat(f, buf);
}

int32_t
sys_close(void)
{
  struct File *f;
  int r, fd;

  if ((r = sys_arg_fd(0, &fd, &f)) < 0)
    return r;

  file_close(f);
  my_process()->files[fd] = NULL;

  return 0;
}

int32_t
sys_read(void)
{
  void *buf;
  size_t n;
  struct File *f;
  int r;

  if ((r = sys_arg_fd(0, NULL, &f)) < 0)
    return r;

  if ((r = sys_arg_int(2, (int *) &n)) < 0)
    return r;

  if ((r = sys_arg_buf(1, &buf, n, VM_U)) < 0)
    return r;

  return file_read(f, buf, n);
}

int32_t
sys_write(void)
{
  void *buf;
  size_t n;
  struct File *f;
  int r;

  if ((r = sys_arg_fd(0, NULL, &f)) < 0)
    return r;

  if ((r = sys_arg_int(2, (int *) &n)) < 0)
    return r;

  if ((r = sys_arg_buf(1, &buf, n, VM_U | VM_W)) < 0)
    return r;

  return file_write(f, buf, n);
}

int32_t
sys_sbrk(void)
{
  ptrdiff_t n;
  int r;

  if ((r = sys_arg_long(0, &n)) < 0)
    return r;
  
  return (int32_t) process_grow(n);
}
