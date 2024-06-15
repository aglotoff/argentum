#include <kernel/assert.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>

#include <kernel/cprintf.h>
#include <kernel/fs/fs.h>
#include <kernel/object_pool.h>
#include <kernel/process.h>
#include <kernel/types.h>

#include "ext2.h"

struct PathNode *fs_root;

static struct KObjectPool *path_pool;
static struct KSpinLock path_lock = K_SPINLOCK_INITIALIZER("path");

struct PathNode *
fs_path_create(const char *name, struct Inode *inode, struct PathNode *parent)
{
  struct PathNode *path;

  if ((path = (struct PathNode *) k_object_pool_get(path_pool)) == NULL)
    return NULL;

  if (name != NULL)
    strncpy(path->name, name, NAME_MAX);

  path->inode = inode;
  path->ref_count = 1;
  path->parent = NULL;
  path->mounted = NULL;
  list_init(&path->children);
  list_init(&path->siblings);
  k_mutex_init(&path->mutex, "path");

  if (parent) {
    k_spinlock_acquire(&path_lock);

    path->parent = parent;
    parent->ref_count++;
    
    list_add_front(&parent->children, &path->siblings);
    path->ref_count++;

    k_spinlock_release(&path_lock);
  }

  return path;
}

struct PathNode *
fs_path_duplicate(struct PathNode *path)
{
  k_spinlock_acquire(&path_lock);
  path->ref_count++;
  k_spinlock_release(&path_lock);

  // cprintf("[%s: %d]\n", path->name, path->ref_count);

  return path;
}

void
fs_path_remove(struct PathNode *path)
{
  k_spinlock_acquire(&path_lock);

  if (path->parent) {
    path->parent->ref_count--;
    path->parent = NULL;
  }

  list_remove(&path->siblings);
  path->ref_count--;

  k_spinlock_release(&path_lock);
}

void
fs_path_put(struct PathNode *path)
{
  k_spinlock_acquire(&path_lock);

  path->ref_count--;

  // Move up the tree and remove all unused nodes. A node is considered unused
  // in one of two cases:
  // a) the reference count is 0
  // b) the reference count is 1, and it's referenced only by the parent node
  while ((path != NULL) && (path->ref_count < 2)) {
    struct PathNode *parent = path->parent;

    if ((path->ref_count == 1) && (parent == NULL))
      // Only one link left, and this is not the parent node
      break;

    if (parent) {
      assert(path->ref_count == 1);

      list_remove(&path->siblings);
      parent->ref_count--;
    }

    k_spinlock_release(&path_lock);

    //cprintf("[drop %s %p]\n", path->name, path->inode);

    if (path->inode != NULL)
      fs_inode_put(path->inode);

    k_object_pool_put(path_pool, path);
    
    k_spinlock_acquire(&path_lock);

    path = parent;
  }

  k_spinlock_release(&path_lock);
}

void
fs_path_lock(struct PathNode *node)
{
  if ((node->ref_count == 1) && (node->parent != NULL))
    panic("bad path node reference");
  k_mutex_lock(&node->mutex);
}

void
fs_path_unlock(struct PathNode *node)
{
  if ((node->ref_count == 1) && (node->parent != NULL))
    panic("bad path node reference");
  k_mutex_unlock(&node->mutex);
}

int
fs_path_next(const char *path, char *name_buf, char **result)
{
  const char *end;
  int n;

  // Skip leading slashes
  while (*path == '/')
    path++;
  
  // This was the last path segment
  if (*path == '\0')
    return 0;

  // Find the end of the path segment
  for (end = path, n = 0; (*end != '\0') && (*end != '/'); end++, n++)
    if (n > NAME_MAX)
      return -ENAMETOOLONG;

  strncpy(name_buf, path, n);
  name_buf[n] = '\0';

  // Skip trailing slashes
  while (*end == '/')
    end++;

  if (result)
    *result = (char *) end;

  return n;
}

static struct PathNode *
fs_path_lookup_cached(struct PathNode *parent, const char *name)
{
  struct ListLink *l;

  k_spinlock_acquire(&path_lock);

  LIST_FOREACH(&parent->children, l) {
    struct PathNode *p = LIST_CONTAINER(l, struct PathNode, siblings);
    
    if (strcmp(p->name, name) == 0) {
      p->ref_count++;
      k_spinlock_release(&path_lock);
      return p;
    }
  }

  k_spinlock_release(&path_lock);
  return NULL;
}

int
fs_path_lookup_at(struct PathNode *start, const char *path, char *name_buf,
                  int real, struct PathNode **istore, struct PathNode **pstore)
{
  struct PathNode *parent, *current;
  int r;

  if (*path == '\0')
    return -ENOENT;

  // For absolute paths, begin search from the root directory.
  // For relative paths, begin search from the specifed starting directory.
  current = fs_path_duplicate(*path == '/' ? fs_root : start);
  parent  = NULL;

  while ((r = fs_path_next(path, name_buf, (char **) &path)) > 0) {
    struct Inode *inode;

    // Stay in the current directory
    if (strcmp(name_buf, ".") == 0)
      continue;

    if (parent != NULL)
      fs_path_put(parent);

    parent  = current;
    current = NULL;

    fs_path_lock(parent);

    // Move to the parent directory
    if (strcmp(name_buf, "..") == 0) {
      current = parent->parent;
      fs_path_unlock(parent);

      if (current == NULL) {
        // TODO: dangling node?
        break;
      }

      fs_path_duplicate(current);

      continue;
    }

    if ((current = fs_path_lookup_cached(parent, name_buf)) != NULL) {
      fs_path_unlock(parent);
      continue;
    }

    r = fs_inode_lookup(parent->inode, name_buf, real, &inode);

    if (r == 0 && inode != NULL) {
      if ((current = fs_path_create(name_buf, inode, parent)) == NULL) {
        fs_inode_put(inode);
        r = -ENOMEM;
      }
    } else {
      current = NULL;
    }

    fs_path_unlock(parent);

    if (r < 0)
      break;

    if (*path == '\0')
      break;

    if (current == NULL) {
      r = -ENOENT;
      break;
    }
  }

  if ((r == 0) && (pstore != NULL))
    *pstore = parent;
  else if (parent != NULL)
    fs_path_put(parent);
  
  if ((r == 0) && (istore != NULL))
    *istore = current;
  else if (current != NULL)
    fs_path_put(current);

  return r;
}

int
fs_path_lookup(const char *path, char *name_buf, int real, 
               struct PathNode **istore, struct PathNode **pstore)
{
  return fs_path_lookup_at(process_current()->cwd, path, name_buf, real,
                           istore, pstore);
}

int
fs_lookup(const char *path, int real, struct PathNode **pp)
{
  char name_buf[NAME_MAX + 1];

  if (strcmp(path, "/") == 0) {
    *pp = fs_path_duplicate(fs_root);
    return 0;
  }

  return fs_path_lookup(path, name_buf, real, pp, NULL);
}

#define FS_ROOT_DEV 0

void
fs_init(void)
{ 
  fs_inode_cache_init();

  path_pool = k_object_pool_create("path_pool", sizeof(struct PathNode), 0,
                                 NULL, NULL);
  if (path_pool == NULL)
    panic("cannot allocate path_pool");

  if ((fs_root = fs_path_create("/", ext2_mount(FS_ROOT_DEV), NULL)) == NULL)
    panic("cannot allocate fs root");

  fs_root->parent = fs_path_duplicate(fs_root);
}

int
fs_access(const char *path, int amode)
{
  struct PathNode *pp;
  int r;

  if ((r = fs_lookup(path, 0, &pp)) < 0)
    return r;

  if (pp == NULL)
    return -ENOENT;

  r = amode != F_OK ? fs_inode_access(pp->inode, amode) : 0;

  fs_path_put(pp);

  return r;
}

ssize_t
fs_readlink(const char *path, char *buf, size_t bufsize)
{
  struct PathNode *pp;
  int r;

  if ((r = fs_lookup(path, 0, &pp)) < 0)
    return r;

  if (pp == NULL)
    return -ENOENT;

  r = fs_inode_readlink(pp->inode, buf, bufsize);

  fs_path_put(pp);

  return r;
}
