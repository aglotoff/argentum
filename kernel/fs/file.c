#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

#include <kernel/drivers/console.h>
#include <kernel/fs/ext2.h>
#include <kernel/fs/fs.h>
#include <kernel/mm/kobject.h>
#include <kernel/sync.h>

#include <kernel/fs/file.h>

static struct SpinLock file_lock;
static struct KObjectPool *file_pool;

void
file_init(void)
{
  if (!(file_pool = kobject_pool_create("file_pool", sizeof(struct File), 0)))
    panic("Cannot allocate file pool");

  spin_init(&file_lock, "file_lock");
}

int
file_open(const char *path, int oflag, struct File **fstore)
{
  struct File *f;
  int r;

  if ((f = (struct File *) kobject_alloc(file_pool)) == NULL)
    return -ENOMEM;
  
  f->type      = 0;
  f->ref_count = 0;
  f->readable  = 0;
  f->writeable = 0;
  f->offset    = 0;
  f->inode     = NULL;

  struct Inode *ip;

  f->type = FD_INODE;

  if ((r = fs_name_lookup(path, &ip)) < 0) 
    goto fail;

  fs_inode_lock(ip);
  if (S_ISDIR(ip->mode) && (oflag & O_WRONLY)) {
    fs_inode_unlock(ip);
    fs_inode_put(ip);

    r = -ENOTDIR;
    goto fail;
  }

  f->inode = ip;

  fs_inode_unlock(ip);

  if (oflag & O_RDONLY)
    f->readable = 1;
  if (oflag & O_WRONLY)
    f->writeable = 1;

  f->ref_count++;

  *fstore = f;

  return 0;

fail:
  kobject_free(file_pool, f);
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

  kobject_free(file_pool, f);
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
    if ((r = fs_inode_read(f->inode, buf, nbytes, f->offset)) > 0)
      f->offset += r;
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
  int r;

  if (!f->readable)
    return -EBADF;
  
  switch (f->type) {
  case FD_INODE:
    assert(f->inode != NULL);
    fs_inode_lock(f->inode);
    r = fs_inode_getdents(f->inode, buf, nbytes, &f->offset);
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
file_write(struct File *f, const void *buf, size_t nbytes)
{
  int r;
  
  if (!f->writeable)
    return -EBADF;

  switch (f->type) {
  case FD_INODE:
    assert(f->inode != NULL);
    fs_inode_lock(f->inode);
    if ((r = fs_inode_write(f->inode, buf, nbytes, f->offset)) > 0)
      f->offset += r;
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
