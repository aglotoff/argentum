#include <kernel/core/assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include <kernel/console.h>
#include <kernel/tty.h>
#include <kernel/time.h>
#include <kernel/fs/buf.h>
#include <kernel/fs/fs.h>
#include <kernel/process.h>
#include <kernel/vmspace.h>
#include <kernel/core/tick.h>

#include "ext2.h"

static struct {
  struct Inode    buf[INODE_CACHE_SIZE];
  struct KSpinLock lock;
  struct KListLink head;
} inode_cache;

void
fs_inode_cache_init(void)
{
  struct Inode *ip;
  
  k_spinlock_init(&inode_cache.lock, "inode_cache");
  k_list_init(&inode_cache.head);

  for (ip = inode_cache.buf; ip < &inode_cache.buf[INODE_CACHE_SIZE]; ip++) {
    k_mutex_init(&ip->mutex, "inode");
    k_list_add_back(&inode_cache.head, &ip->cache_link);
  }
}

struct Inode *
fs_inode_get(ino_t ino, dev_t dev)
{
  struct KListLink *l;
  struct Inode *ip, *empty;

  k_spinlock_acquire(&inode_cache.lock);

  empty = NULL;
  KLIST_FOREACH(&inode_cache.head, l) {
    ip = KLIST_CONTAINER(l, struct Inode, cache_link);
    if ((ip->ino == ino) && (ip->dev == dev) && (ip->ref_count > 0)) {
      ip->ref_count++;

      k_spinlock_release(&inode_cache.lock);

      return ip;
    }

    if (ip->ref_count == 0)
      empty = ip;
  }

  if (empty != NULL) {
    empty->ref_count = 1;
    empty->ino       = ino;
    empty->dev       = dev;
    empty->fs        = NULL;
    empty->flags     = 0;

    k_spinlock_release(&inode_cache.lock);

    return empty;
  }

  k_spinlock_release(&inode_cache.lock);

  k_panic("out of inodes\n");

  return NULL;
}

/**
 * Increment the reference counter of the given inode.
 * 
 * @param inode Pointer to the inode
 * 
 * @return Pointer to the inode.
 */
struct Inode *
fs_inode_duplicate(struct Inode *inode)
{
  k_spinlock_acquire(&inode_cache.lock);
  inode->ref_count++;
  k_spinlock_release(&inode_cache.lock);

  return inode;
}

/**
 * Release pointer to an inode.
 * 
 * @param inode Pointer to the inode to be released
 */
void
fs_inode_put(struct Inode *inode)
{   
  int r;

  if ((r = k_mutex_lock(&inode->mutex)) < 0)
    k_panic("TODO %d", r);

  if (inode->flags & FS_INODE_DIRTY)
    k_panic("inode dirty");

  // If the link count reaches zero, delete inode from the filesystem before
  // returning it to the cache
  if ((inode->flags & FS_INODE_VALID) && (inode->nlink == 0)) {
    int ref_count;

    k_spinlock_acquire(&inode_cache.lock);
    ref_count = inode->ref_count;
    k_spinlock_release(&inode_cache.lock);

    // If this is the last reference to this inode
    if (ref_count == 1) {
      inode->fs->ops->inode_delete(thread_current(), inode);
      inode->flags &= ~FS_INODE_VALID;
    }
  }

  k_mutex_unlock(&inode->mutex);

  // Return the inode to the cache
  k_spinlock_acquire(&inode_cache.lock);
  if (--inode->ref_count == 0) {
    // cprintf("[inode drop %d]\n", inode->ino);

    k_list_remove(&inode->cache_link);
    k_list_add_front(&inode_cache.head, &inode->cache_link);
  }
  k_spinlock_release(&inode_cache.lock);
}

int
fs_inode_holding(struct Inode *ip)
{
  return k_mutex_holding(&ip->mutex);
}

/**
 * Lock the given inode. Read the inode meta info, if necessary.
 * 
 * @param ip Pointer to the inode.
 */
void
fs_inode_lock(struct Inode *ip)
{
  int r;

  if ((r = k_mutex_lock(&ip->mutex)) < 0)
    k_panic("TODO %d", r);

  if (ip->flags & FS_INODE_VALID)
    return;

  if (ip->flags & FS_INODE_DIRTY)
    k_panic("inode dirty");

  ip->fs->ops->inode_read(thread_current(), ip);

  //msg.type = FS_MSG_INODE_READ;
  //msg.u.inode_read.inode = ip;

  //fs_send_recv(ip->fs, &msg);

  ip->flags |= FS_INODE_VALID;
}

void
fs_inode_unlock(struct Inode *ip)
{  
  if (!(ip->flags & FS_INODE_VALID))
    k_panic("inode not valid");

  if (ip->flags & FS_INODE_DIRTY) {
    ip->fs->ops->inode_write(thread_current(), ip);
    ip->flags &= ~FS_INODE_DIRTY;
  }

  k_mutex_unlock(&ip->mutex);
}

void
fs_inode_lock_two(struct Inode *inode1, struct Inode *inode2)
{
  if (inode1 < inode2) {
    fs_inode_lock(inode1);
    fs_inode_lock(inode2);
  } else {
    fs_inode_lock(inode2);
    fs_inode_lock(inode1);
  }
}

void
fs_inode_unlock_two(struct Inode *inode1, struct Inode *inode2)
{
  if (inode1 < inode2) {
    fs_inode_unlock(inode2);
    fs_inode_unlock(inode1);
  } else {
    fs_inode_unlock(inode1);
    fs_inode_unlock(inode2);
  }
}

int
fs_inode_permission(struct Thread *thread, struct Inode *inode, mode_t mode, int real)
{
  struct Process *my_process = thread ? thread->process : NULL;

  uid_t uid = my_process ? (real ? my_process->ruid : my_process->euid) : 0;
  gid_t gid = my_process ? (real ? my_process->rgid : my_process->egid) : 0;

  if (uid == 0)
    return (mode & FS_PERM_EXEC)
      ? ((inode->mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0)
      : 1;

  // TODO: support S_ISVTX

  if (uid == inode->uid)
    mode <<= 6;
  else if (gid == inode->gid)
    mode <<= 3;

  return (inode->mode & mode) == mode;
}
