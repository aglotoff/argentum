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

static int
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
  //struct FSMessage msg;

  if (k_mutex_lock(&ip->mutex) < 0)
    k_panic("TODO");

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

    // struct FSMessage msg;

    // msg.type = FS_MSG_INODE_WRITE;
    // msg.u.inode_read.inode = ip;

    // fs_send_recv(ip->fs, &msg);

    ip->flags &= ~FS_INODE_DIRTY;
  }

  k_mutex_unlock(&ip->mutex);
}

int
fs_inode_link(struct Inode *inode, struct Inode *dir, char *name)
{
  // FIXME: works only in the same filesystem!
  struct FSMessage msg;

  if (!fs_inode_holding(inode))
    k_panic("inode not locked");
  if (!fs_inode_holding(dir))
    k_panic("directory not locked");

  msg.type = FS_MSG_LINK;
  msg.u.link.dir   = dir;
  msg.u.link.name  = name;
  msg.u.link.inode = inode;

  fs_send_recv(dir->fs, &msg);

  return msg.u.link.r;
}

int
fs_inode_lookup(struct Inode *dir_inode, const char *name, int flags,
                struct Inode **inode_store)
{
  struct FSMessage msg;

  msg.type = FS_MSG_LOOKUP;
  msg.u.lookup.dir    = dir_inode;
  msg.u.lookup.name   = name;
  msg.u.lookup.flags  = flags;
  msg.u.lookup.istore = inode_store;

  fs_inode_lock(dir_inode);
  fs_send_recv(dir_inode->fs, &msg);
  fs_inode_unlock(dir_inode);

  return msg.u.lookup.r;
}

int
fs_inode_unlink(struct Inode *dir, struct Inode *inode, const char *name)
{
  struct FSMessage msg;

  if (!fs_inode_holding(inode))
    k_panic("inode not locked");
  if (!fs_inode_holding(dir))
    k_panic("directory not locked");

  msg.type = FS_MSG_UNLINK;
  msg.u.unlink.dir   = dir;
  msg.u.unlink.inode = inode;
  msg.u.unlink.name  = name;

  fs_send_recv(dir->fs, &msg);

  return msg.u.unlink.r;
}

int
fs_inode_rmdir(struct Inode *dir, struct Inode *inode, const char *name)
{
  struct FSMessage msg;

  if (!fs_inode_holding(inode))
    k_panic("inode not locked");
  if (!fs_inode_holding(dir))
    k_panic("directory not locked");

  msg.type = FS_MSG_RMDIR;
  msg.u.rmdir.dir   = dir;
  msg.u.rmdir.inode = inode;
  msg.u.rmdir.name  = name;

  fs_send_recv(dir->fs, &msg);

  return msg.u.rmdir.r;
}

static void
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

static void
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
fs_link(char *path1, char *path2)
{
  struct PathNode *dirp, *pp;
  struct Inode *inode, *parent_inode;
  char name[NAME_MAX + 1];
  int r;

  // REF(pp)
  if ((r = fs_path_resolve(path1, 0, &pp)) < 0)
    return r;
  if (pp == NULL)
    return -ENOENT;

  // REF(dirp)
  if ((r = fs_path_node_resolve(path2, name, FS_LOOKUP_FOLLOW_LINKS, NULL, &dirp)) < 0)
    goto out1;

  // TODO: check for the same node?
  // TODO: lock the namespace manager?

  parent_inode = fs_inode_duplicate(fs_path_inode(dirp));
  inode = fs_inode_duplicate(fs_path_inode(pp));

  // Always lock inodes in a specific order to avoid deadlocks
  fs_inode_lock_two(parent_inode, inode);

  r = fs_inode_link(inode, parent_inode, name);

  fs_inode_unlock_two(parent_inode, inode);

  fs_inode_put(inode);
  fs_inode_put(parent_inode);

  // UNREF(dirp)
  fs_path_node_unref(dirp);
out1:
  // UNREF(pp)
  fs_path_node_unref(pp);
  return r;
}

int
fs_rename(char *old, char *new)
{
  struct PathNode *old_dir, *new_dir, *old_node, *new_node;
  struct Inode *inode, *parent_inode;
  char old_name[NAME_MAX + 1];
  char new_name[NAME_MAX + 1];
  int r;

  // REF(old_node), REF(old_dir)
  if ((r = fs_path_node_resolve(old, old_name, FS_LOOKUP_FOLLOW_LINKS, &old_node, &old_dir)) < 0)
    return r;
  if (old_node == NULL)
    return -ENOENT;

  // REF(new_node), REF(new_dir)
  if ((r = fs_path_node_resolve(new, new_name, FS_LOOKUP_FOLLOW_LINKS, &new_node, &new_dir)) < 0)
    goto out1;
  if (new_dir == NULL) {
    r = -ENOENT;
    goto out1;
  }

  // TODO: check for the same node?
  // TODO: lock the namespace manager?
  // TODO: should be a transaction?

  if (new_node != NULL) {
    parent_inode = fs_inode_duplicate(fs_path_inode(new_dir));
    inode = fs_inode_duplicate(fs_path_inode(new_node));

    // TODO: lock the namespace manager?

    fs_inode_lock_two(parent_inode, inode);
    
    // FIXME: check if is dir

    if ((r = fs_inode_unlink(parent_inode, inode, new_name)) == 0) {
      fs_path_node_remove(new_node);
    }

    fs_inode_unlock_two(parent_inode, inode);

    fs_inode_put(inode);
    fs_inode_put(parent_inode);

    // UNREF(new_node)
    fs_path_node_unref(new_node);
    new_node = NULL;

    if (r != 0)
      goto out2;
  }

  inode        = fs_inode_duplicate(fs_path_inode(old_node));
  parent_inode = fs_inode_duplicate(fs_path_inode(new_dir));

  fs_inode_lock_two(parent_inode, inode);
  r = fs_inode_link(inode, parent_inode, new_name);
  fs_inode_unlock_two(parent_inode, inode);

  fs_inode_put(parent_inode);

  if (r < 0)
    goto out2;

  parent_inode = fs_inode_duplicate(fs_path_inode(old_dir));
    
  fs_inode_lock_two(parent_inode, inode);
  //cprintf("remove %s\n", );
  if ((r = fs_inode_unlink(parent_inode, inode, old_name)) == 0) {
    fs_path_node_remove(old_node);
  }
  fs_inode_unlock_two(parent_inode, inode);

  fs_inode_put(parent_inode);

out2:
  fs_inode_put(inode);

  // UNREF(new_dir)
  fs_path_node_unref(new_dir);

out1:
  // UNREF(old_dir)
  if (old_dir != NULL)
    fs_path_node_unref(old_dir);
  // UNREF(old_node)
  if (old_node != NULL)
    fs_path_node_unref(old_node);
  return r;
}

int
fs_unlink(const char *path)
{
  struct PathNode *dir, *pp;
  struct Inode *inode, *parent_inode;
  char name[NAME_MAX + 1];
  int r;

  // REF(pp), REF(dir)
  if ((r = fs_path_node_resolve(path, name, FS_LOOKUP_FOLLOW_LINKS, &pp, &dir)) < 0)
    return r;

  if (pp == NULL) {
    // UNREF(dir)
    fs_path_node_unref(dir);
    return -ENOENT;
  }

  parent_inode = fs_inode_duplicate(fs_path_inode(dir));
  inode        = fs_inode_duplicate(fs_path_inode(pp));

  // TODO: lock the namespace manager?

  fs_inode_lock_two(parent_inode, inode);
  
  if ((r = fs_inode_unlink(parent_inode, inode, pp->name)) == 0) {
    fs_path_node_remove(pp);
  }

  fs_inode_unlock_two(parent_inode, inode);

  fs_inode_put(inode);
  fs_inode_put(parent_inode);

  // UNREF(dir)
  fs_path_node_unref(dir);

  // UNREF(pp)
  fs_path_node_unref(pp);

  return r;
}

int
fs_rmdir(const char *path)
{
  struct PathNode *dir, *pp;
  struct Inode *inode, *parent_inode;
  char name[NAME_MAX + 1];
  int r;

  // REF(pp), REF(dir)
  if ((r = fs_path_node_resolve(path, name, 0, &pp, &dir)) < 0)
    return r;

  if (pp == NULL) {
    // UNREF(dir)
    fs_path_node_unref(dir);
  
    return -ENOENT;
  }

  // TODO: lock the namespace manager?

  parent_inode = fs_inode_duplicate(fs_path_inode(dir));
  inode = fs_inode_duplicate(fs_path_inode(pp));

  fs_inode_lock_two(parent_inode, inode);

  if ((r = fs_inode_rmdir(parent_inode, inode, pp->name)) == 0) {
    fs_path_node_remove(pp);
  }

  fs_inode_unlock_two(parent_inode, inode);

  fs_inode_put(inode);
  fs_inode_put(parent_inode);

  // UNREF(dir)
  fs_path_node_unref(dir);

  // UNREF(pp)
  fs_path_node_unref(pp);

  return r;
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
