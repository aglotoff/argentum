#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

#include <argentum/cprintf.h>
#include <argentum/fs/ext2.h>
#include <argentum/fs/file.h>
#include <argentum/fs/fs.h>
#include <argentum/mm/kmem.h>
#include <argentum/spin.h>

static struct SpinLock file_lock;
static struct KMemCache *file_cache;

void
file_init(void)
{
  if (!(file_cache = kmem_cache_create("file_cache", sizeof(struct File), 0, NULL, NULL)))
    panic("Cannot allocate file cache");

  spin_init(&file_lock, "file_lock");
}

int
file_open(const char *path, int oflag, mode_t mode, struct File **fstore)
{
  struct File *f;
  int r;

  // TODO: ENFILE
  if ((f = (struct File *) kmem_alloc(file_cache)) == NULL)
    return -ENOMEM;
  
  f->type      = 0;
  f->ref_count = 0;
  f->readable  = 0;
  f->writeable = 0;
  f->offset    = 0;
  f->inode     = NULL;

  struct Inode *ip;

  f->type = FD_INODE;

  // TODO: EINTR

  // TODO: the check and the file creation should be atomic
  if ((r = fs_name_lookup(path, &ip)) < 0) {
    if ((r != -ENOENT) || !(oflag & O_CREAT))
      goto fail1;

    mode &= (S_IRWXU | S_IRWXG | S_IRWXO);
    if ((r = fs_create(path, S_IFREG | mode, 0, &ip)) < 0)
      goto fail1;
  } else {
    if ((oflag & O_CREAT) && (oflag & O_EXCL)) {
      r = -EEXISTS;
      goto fail2;
    }

    fs_inode_lock(ip);
  }

  if (S_ISDIR(ip->mode) && (oflag & O_WRONLY)) {
    r = -ENOTDIR;
    goto fail2;
  }

  // TODO: O_NOCTTY
  // TODO: O_NONBLOCK
  // TODO: ENXIO
  // TODO: EROFS

  if ((oflag & O_WRONLY) && (oflag & O_TRUNC)) {
    if ((r = fs_inode_trunc(ip)) < 0)
      goto fail2;
  }

  f->inode = ip;

  if (oflag & O_RDONLY) {
    // TODO: check group and other permissions
    if (!(f->inode->mode & S_IRUSR)) {
      r = -EPERM;
      goto fail2;
    }

    f->readable = 1;
  }

  if (oflag & O_WRONLY) {
    // TODO: check group and other permissions
    if (!(f->inode->mode & S_IWUSR)) {
      r = -EPERM;
      goto fail2;
    }

    f->writeable = 1;
  }

  fs_inode_unlock(ip);

  if (oflag & O_APPEND)
    f->offset = ip->size;

  f->ref_count++;

  *fstore = f;

  return 0;

fail2:
  fs_inode_unlock_put(ip);
fail1:
  kmem_free(file_cache, f);
  return r;
}

struct File *
file_dup(struct File *f)
{
  spin_lock(&file_lock);
  f->ref_count++;
  spin_unlock(&file_lock);

  return f;
}

void
file_close(struct File *f)
{
  int ref_count;
  
  spin_lock(&file_lock);

  if (f->ref_count < 1)
    panic("Invalid ref_count");

  ref_count = --f->ref_count;

  spin_unlock(&file_lock);

  if (ref_count > 0)
    return;

  switch (f->type) {
    case FD_INODE:
      assert(f->inode != NULL);
      fs_inode_put(f->inode);
    case FD_PIPE:
      // TODO: close pipe
      break;
    default:
      panic("Invalid type");
  }

  kmem_free(file_cache, f);
}

ssize_t
file_read(struct File *f, void *buf, size_t nbytes)
{
  int r;

  if (!f->readable)
    return -EBADF;
  
  switch (f->type) {
  case FD_INODE:
    assert(f->inode != NULL);

    fs_inode_lock(f->inode);

    // TODO: check group and other permissions
    // TODO: what if permissions change?
    if (!(f->inode->mode & S_IRUSR)) {
      fs_inode_unlock(f->inode);
      return -EPERM;
    }

    r = fs_inode_read(f->inode, buf, nbytes, &f->offset);

    fs_inode_unlock(f->inode);

    return r;

  case FD_PIPE:
    // TODO: read from a pipe

  default:
    panic("Invalid type");
    return 0;
  }
}

ssize_t
file_getdents(struct File *f, void *buf, size_t nbytes)
{
  ssize_t ret, total;
  char *dst;

  if (!f->readable)
    return -EBADF;

  if ((f->type != FD_INODE) || !S_ISDIR(f->inode->mode))
    return -ENOTDIR;

  dst = (char *) buf;
  total = 0;

  fs_inode_lock(f->inode);

  // TODO: check group and other permissions
  // TODO: what if permissions change?
  if (!(f->inode->mode & S_IRUSR)) {
    fs_inode_unlock(f->inode);
    return -EPERM;
  }

  while (nbytes > 0) {
    if ((ret = ext2_dir_iterate(f->inode, dst, nbytes, &f->offset)) < 0) {
      fs_inode_unlock(f->inode);
      return ret;
    }

    if (ret == 0)
      break;

    dst    += ret;
    total  += ret;
    nbytes -= ret;
  }

  fs_inode_unlock(f->inode);

  return total;
}

ssize_t
file_write(struct File *f, const void *buf, size_t nbytes)
{
  int r;
  
  if (!f->writeable)
    return -EBADF;

  switch (f->type) {
  case FD_INODE:
    assert(f->inode != NULL);

    fs_inode_lock(f->inode);

    // TODO: check group and other permissions
    // TODO: what if permissions change?
    if (!(f->inode->mode & S_IWUSR)) {
      fs_inode_unlock(f->inode);
      return -EPERM;
    }
  
    r = fs_inode_write(f->inode, buf, nbytes, &f->offset);

    fs_inode_unlock(f->inode);

    return r;
  case FD_PIPE:
    // TODO: write to a pipe
  default:
    panic("Invalid type");
    return 0;
  }
}

int
file_stat(struct File *fp, struct stat *buf)
{
  int r;
  
  if (fp->type != FD_INODE) 
    return -EBADF;

  assert(fp->inode != NULL);

  fs_inode_lock(fp->inode);
  r = fs_inode_stat(fp->inode, buf);
  fs_inode_unlock(fp->inode);

  return r;
}
