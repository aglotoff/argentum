#include <dirent.h>
#include <sys/fcntl.h>
#include <sys/stat.h>

#include <kernel/page.h>
#include <kernel/fs/fs.h>
#include <kernel/object_pool.h>
#include <kernel/process.h>
#include <kernel/time.h>
#include <kernel/vmspace.h>

static int
do_inode_read(struct FS *fs, struct Thread *sender, struct Inode *inode)
{
  return fs->ops->inode_read(sender, inode);
}

static int
do_inode_write(struct FS *fs, struct Thread *sender, struct Inode *inode)
{
  return fs->ops->inode_write(sender, inode);
}

static void
do_inode_delete(struct FS *fs, struct Thread *sender, struct Inode *inode)
{
  fs->ops->inode_delete(sender, inode);
}

static int
do_trunc(struct FS *fs, struct Thread *sender,
         struct Inode *inode,
         off_t length)
{
  if (!fs_permission(sender, inode, FS_PERM_WRITE, 0))
    return -EPERM;

  fs->ops->trunc(sender, inode, length);

  inode->size = length;
  inode->ctime = inode->mtime = time_get_seconds();

  return 0;
}

static int
do_create(struct FS *fs, struct Thread *sender,
          struct Inode *dir,
          char *name,
          mode_t mode,
          dev_t dev,
          struct Inode **istore)
{
  struct Inode *inode;

  if (!S_ISDIR(dir->mode))
    return -ENOTDIR;

  if (!fs_permission(sender, dir, FS_PERM_WRITE, 0))
    return -EPERM;

  inode = fs->ops->lookup(sender, dir, name);

  if (inode != NULL) {
    fs_inode_put(inode);
    return -EEXIST;
  }

  switch (mode & S_IFMT) {
    case S_IFDIR:
      return fs->ops->mkdir(sender, dir, name, mode, istore);
    case S_IFREG:
      return fs->ops->create(sender, dir, name, mode, istore);
    default:
      return fs->ops->mknod(sender, dir, name, mode, dev, istore);
  }
}

static ssize_t
do_read(struct FS *fs, struct Thread *sender,
        struct Inode *inode,
        uintptr_t va,
        size_t nbyte,
        off_t off)
{
  ssize_t total;

  if ((off_t) (off + nbyte) < off)
    return -EINVAL;

  if ((off_t) (off + nbyte) > inode->size)
    nbyte = inode->size - off;
  if (nbyte == 0)
    return 0;

  total = fs->ops->read(sender, inode, va, nbyte, off);

  if (total >= 0)
    inode->atime  = time_get_seconds();

  return total;
}

static ssize_t
do_write(struct FS *fs, struct Thread *sender,
         struct Inode *inode,
         uintptr_t va,
         size_t nbyte,
         off_t off)
{
  ssize_t total;

  if ((off_t) (off + nbyte) < off)
    return -EINVAL;

  if (nbyte == 0)
    return 0;

  total = fs->ops->write(sender, inode, va, nbyte, off);

  if (total > 0) {
    off += total;

    if (off > inode->size)
      inode->size = off;

    inode->mtime = time_get_seconds();
  }

  return total;
}

static int
fs_filldir(void *buf, ino_t ino, const char *name, size_t name_len)
{
  struct dirent *dp = (struct dirent *) buf;

  dp->d_reclen  = name_len + offsetof(struct dirent, d_name) + 1;
  dp->d_ino     = ino;
  memmove(&dp->d_name[0], name, name_len);
  dp->d_name[name_len] = '\0';

  return dp->d_reclen;
}

static ssize_t
do_readdir(struct FS *fs, struct Thread *sender,
           struct Inode *inode,
           uintptr_t va,
           size_t nbyte,
           off_t *off)
{
  ssize_t total = 0;

  struct {
    struct dirent de;
    char buf[NAME_MAX + 1];
  } de;

  if (!S_ISDIR(inode->mode))
    return -ENOTDIR;

  if (!fs_permission(sender, inode, FS_PERM_READ, 0))
    return -EPERM;

  while (nbyte > 0) {
    ssize_t nread;
    int r;

    nread = fs->ops->readdir(sender, inode, &de, fs_filldir, *off);

    if (nread < 0)
      return nread;

    if (nread == 0)
      break;
    
    if (de.de.d_reclen > nbyte) {
      if (total == 0) {
        return -EINVAL;
      }
      break;
    }

    *off += nread;

    if ((r = vm_space_copy_out(sender, &de, va, de.de.d_reclen)) < 0)
      return r;

    va    += de.de.d_reclen;
    total += de.de.d_reclen;
    nbyte -= de.de.d_reclen;
  }

  return total;
}

static int
do_link(struct FS *fs, struct Thread *sender,
        struct Inode *dir,
        char *name,
        struct Inode *inode)
{
  if (!S_ISDIR(dir->mode))
    return -ENOTDIR;

  if (!fs_permission(sender, dir, FS_PERM_WRITE, 0))
    return -EPERM;

  // TODO: Allow links to directories?
  if (S_ISDIR(inode->mode))
    return -EPERM;

  if (inode->nlink >= LINK_MAX)
    return -EMLINK;

  if (dir->dev != inode->dev)
    return -EXDEV;

  return fs->ops->link(sender, dir, name, inode);
}

static int
do_lookup(struct FS *fs, struct Thread *sender,
          struct Inode *dir,
          const char *name,
          int flags,
          struct Inode **inode_store)
{
  struct Inode *inode;

  if (!S_ISDIR(dir->mode))
    return -ENOTDIR;

  if (!fs_permission(sender, dir, FS_PERM_READ, flags & FS_LOOKUP_REAL))
    return -EPERM;

  inode = fs->ops->lookup(sender, dir, name);

  if (inode_store != NULL)
    *inode_store = inode;
  else if (inode != NULL)
    fs_inode_put(inode);

  return 0;
}

static int
do_unlink(struct FS *fs, struct Thread *sender,
          struct Inode *dir,
          struct Inode *inode,
          const char *name)
{
  if (!S_ISDIR(dir->mode))
    return -ENOTDIR;

  if (!fs_permission(sender, dir, FS_PERM_WRITE, 0))
    return -EPERM;

  // TODO: Allow links to directories?
  if (S_ISDIR(inode->mode))
    return -EPERM;

  return fs->ops->unlink(sender, dir, inode, name);
}

static int
do_rmdir(struct FS *fs, struct Thread *sender,
         struct Inode *dir,
         struct Inode *inode,
         const char *name)
{
  if (!S_ISDIR(dir->mode))
    return -ENOTDIR;

  if (!fs_permission(sender, dir, FS_PERM_WRITE, 0))
    return -EPERM;

  // TODO: Allow links to directories?
  if (!S_ISDIR(inode->mode))
    return -EPERM;

  return fs->ops->rmdir(sender, dir, inode, name);
}

static ssize_t
do_readlink(struct FS *fs, struct Thread *sender,
            struct Inode *inode,
            uintptr_t va,
            size_t nbyte)
{
  ssize_t r;

  if (!fs_permission(sender, inode, FS_PERM_READ, 0))
    return -EPERM;
    
  if (!S_ISLNK(inode->mode))
    return -EINVAL;

  r = fs->ops->readlink(sender, inode, va, nbyte);

  return r;
}

void
fs_service_task(void *arg)
{
  struct FS *fs = (struct FS *) arg;
  struct FSMessage *msg;

  while (k_mailbox_receive(&fs->mbox, (void *) &msg) >= 0) {
    k_assert(msg != NULL);

    switch (msg->type) {
    case FS_MSG_INODE_READ:
      msg->u.inode_read.r = do_inode_read(fs, msg->sender,
                                          msg->u.inode_read.inode);
      break;
    case FS_MSG_INODE_WRITE:
      msg->u.inode_write.r = do_inode_write(fs, msg->sender,
                                            msg->u.inode_write.inode);
      break;
    case FS_MSG_INODE_DELETE:
      do_inode_delete(fs, msg->sender,
                      msg->u.inode_delete.inode);
      break;
    case FS_MSG_TRUNC:
    msg->u.trunc.r = do_trunc(fs, msg->sender,
                              msg->u.trunc.inode,
                              msg->u.trunc.length);
      break;
    case FS_MSG_LOOKUP:
      msg->u.lookup.r = do_lookup(fs, msg->sender,
                                  msg->u.lookup.dir,
                                  msg->u.lookup.name,
                                  msg->u.lookup.flags,
                                  msg->u.lookup.istore);
      break;
    case FS_MSG_READ:
      msg->u.read.r = do_read(fs, msg->sender,
                              msg->u.read.inode,
                              msg->u.read.va, msg->u.read.nbyte,
                              msg->u.read.off);
      break;
    case FS_MSG_WRITE:
      msg->u.write.r = do_write(fs, msg->sender,
                                msg->u.write.inode,
                                msg->u.write.va,
                                msg->u.write.nbyte,
                                msg->u.write.off);
      break;
    case FS_MSG_READDIR:
      msg->u.readdir.r = do_readdir(fs, msg->sender,
                                    msg->u.readdir.inode,
                                    msg->u.readdir.va,
                                    msg->u.readdir.nbyte,
                                    msg->u.readdir.off);
      break;
    case FS_MSG_CREATE:
      msg->u.create.r = do_create(fs, msg->sender,
                                  msg->u.create.dir,
                                  msg->u.create.name,
                                  msg->u.create.mode,
                                  msg->u.create.dev,
                                  msg->u.create.istore);
      break;
    case FS_MSG_LINK:
      msg->u.link.r = do_link(fs, msg->sender,
                              msg->u.link.dir,
                              msg->u.link.name,
                              msg->u.link.inode);
      break;
    case FS_MSG_UNLINK:
      msg->u.unlink.r = do_unlink(fs, msg->sender,
                                  msg->u.unlink.dir,
                                  msg->u.unlink.inode,
                                  msg->u.unlink.name);
      break;    
    case FS_MSG_RMDIR:
      msg->u.rmdir.r = do_rmdir(fs, msg->sender,
                                msg->u.rmdir.dir,
                                msg->u.rmdir.inode,
                                msg->u.rmdir.name);
      break;
    case FS_MSG_READLINK:
      msg->u.readlink.r = do_readlink(fs, msg->sender,
                                      msg->u.readlink.inode,
                                      msg->u.readlink.va,
                                      msg->u.readlink.nbyte);
      break;
    default:
      k_panic("bad msg type %d\n", msg->type);
      break;
    }

    k_semaphore_put(&msg->sem);
  }

  k_panic("error");
}

struct FS *
fs_create_service(char *name, dev_t dev, void *extra, struct FSOps *ops)
{
  struct FS *fs;
  int i;

  if ((fs = (struct FS *) k_malloc(sizeof(struct FS))) == NULL)
    k_panic("cannot allocate FS");
  
  fs->name  = name;
  fs->dev   = dev;
  fs->extra = extra;
  fs->ops   = ops;

  k_mailbox_create(&fs->mbox, sizeof(void *), fs->mbox_buf, sizeof fs->mbox_buf);

  for (i = 0; i < FS_MBOX_CAPACITY; i++) {
    struct Page *kstack;

    if ((kstack = page_alloc_one(0, 0)) == NULL)
      k_panic("out of memory");

    kstack->ref_count++;

    k_task_create(&fs->tasks[i], NULL, fs_service_task, fs, page2kva(kstack), PAGE_SIZE, 0);
    k_task_resume(&fs->tasks[i]);
  }

  return fs;
}

int
fs_send_recv(struct FS *fs, struct FSMessage *msg)
{
  struct FSMessage *msg_ptr = msg;
  unsigned long long timeout = seconds2ticks(10);
  int r;

  k_semaphore_create(&msg->sem, 0);
  msg->sender = thread_current();

  if (k_mailbox_timed_send(&fs->mbox, &msg_ptr, timeout) < 0)
    k_panic("fail send %d: %d", msg->type, r);

  if ((r = k_semaphore_timed_get(&msg->sem, timeout, K_SLEEP_UNINTERUPTIBLE)) < 0)
    k_panic("fail recv %d: %d", msg->type, r);

  return 0;
}
