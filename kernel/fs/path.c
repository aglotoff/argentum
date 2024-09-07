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

#include "devfs.h"
#include "ext2.h"

struct PathNode *fs_root;

static struct KObjectPool *fs_path_pool;
static struct KSpinLock fs_path_lock = K_SPINLOCK_INITIALIZER("fs_path");

static void
fs_path_node_ctor(void *ptr, size_t n)
{
  struct PathNode *path_node = (struct PathNode *) ptr;
  (void) n;

  path_node->mounted = NULL;
  path_node->ref_count = 0;
  k_list_init(&path_node->children);
  k_list_null(&path_node->siblings);
  k_mutex_init(&path_node->mutex, "path_node");
}

static void
fs_path_node_dtor(void *ptr, size_t n)
{
  struct PathNode *path_node = (struct PathNode *) ptr;
  (void) n;

  assert(path_node->mounted == NULL);
  assert(path_node->ref_count == 0);
  assert(k_list_is_empty(&path_node->children));
  assert(k_list_is_empty(&path_node->siblings));
  assert(!k_mutex_holding(&path_node->mutex));
}

struct PathNode *
fs_path_node_create(const char *name, struct Inode *inode,
                    struct PathNode *parent)
{
  struct PathNode *path;

  if ((path = (struct PathNode *) k_object_pool_get(fs_path_pool)) == NULL)
    return NULL;

  if (name != NULL)
    strncpy(path->name, name, NAME_MAX);

  path->inode  = inode;
  path->parent = parent;
  path->ref_count++;

  if (parent) {
    k_spinlock_acquire(&fs_path_lock);
  
    parent->ref_count++;
    
    k_list_add_front(&parent->children, &path->siblings);
    path->ref_count++;

    k_spinlock_release(&fs_path_lock);
  }

  //cprintf("[create %s %d]\n", name, path->ref_count);

  return path;
}

struct PathNode *
fs_path_duplicate(struct PathNode *path)
{
  k_spinlock_acquire(&fs_path_lock);
  path->ref_count++;
  k_spinlock_release(&fs_path_lock);

  // cprintf("[dup %s]\n", path);

  return path;
}

int
fs_path_mount(struct PathNode *path, struct Inode *inode)
{
  if (path->mounted)
    panic("already mounted");

  path->mounted = inode;

  // TODO: remove all cached entries???

  return 0;
}

void
fs_path_remove(struct PathNode *path)
{
  k_spinlock_acquire(&fs_path_lock);

  if (path->parent) {
    path->parent->ref_count--;
    path->parent = NULL;
  }

  k_list_remove(&path->siblings);
  path->ref_count--;

  k_spinlock_release(&fs_path_lock);
}

void
fs_path_put(struct PathNode *path)
{
  k_spinlock_acquire(&fs_path_lock);

  path->ref_count--;

  // cprintf("[put %s %d]\n", path->name, path->ref_count);

  if ((path->ref_count == 0) && (path->parent != NULL))
    panic("path in bad state");

  // Move up the tree and remove all unused nodes. A node is considered unused
  // in one of two cases:
  // a) the reference count is 0
  // b) the reference count is 1, and it's referenced only by the parent node
  // TODO: parent of unmounted entry??
  while ((path != NULL) && (path->ref_count < 2)) {
    struct PathNode *parent = path->parent;

    if ((path->ref_count == 1) && (parent == NULL))
      // Only one link left, and this is not the parent node
      break;

    if (parent) {
      assert(path->ref_count == 1);
      k_list_remove(&path->siblings);
      path->ref_count--;
    }

    k_spinlock_release(&fs_path_lock);

    // cprintf("[drop %s %d]\n", path->name, path->inode->ino);

    if (path->mounted != NULL)
      panic("TODO: drop mountpoint");

    if (path->inode != NULL)
      fs_inode_put(path->inode);

    k_object_pool_put(fs_path_pool, path);
    
    k_spinlock_acquire(&fs_path_lock);

    if (parent)
      parent->ref_count--;

    path = parent;
  }

  k_spinlock_release(&fs_path_lock);
}

struct Inode *
fs_path_inode(struct PathNode *node)
{
  struct Inode *inode;

  k_spinlock_acquire(&fs_path_lock);
  inode = node->mounted ? node->mounted : node->inode;
  k_spinlock_release(&fs_path_lock);

  return fs_inode_duplicate(inode);
}

void
fs_fs_path_node_lock(struct PathNode *node)
{
  if ((node->ref_count == 1) && (node->parent != NULL))
    panic("bad path node reference");
  k_mutex_lock(&node->mutex);
}

void
fs_path_node_unlock(struct PathNode *node)
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
  struct KListLink *l;

  // TODO: this is a bottleneck! 
  // if there are many child nodes, comparing all names may take too long, and
  // we're blocking the entire system. Could we use a per-node mutex instead?

  k_spinlock_acquire(&fs_path_lock);

  KLIST_FOREACH(&parent->children, l) {
    struct PathNode *p = KLIST_CONTAINER(l, struct PathNode, siblings);
    
    if (strcmp(p->name, name) == 0) {
      p->ref_count++;
      k_spinlock_release(&fs_path_lock);
      return p;
    }
  }

  k_spinlock_release(&fs_path_lock);
  return NULL;
}

int
fs_path_lookup_at(struct PathNode *start,
                  const char *path,
                  char *name_buf,
                  int flags,
                  struct PathNode **store,
                  struct PathNode **parent_store)
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
    struct Inode *inode, *parent_inode;

    // Stay in the current directory
    if (strcmp(name_buf, ".") == 0)
      continue;

    if (parent != NULL)
      fs_path_put(parent);

    parent  = current;
    current = NULL;

    fs_fs_path_node_lock(parent);

    // Move to the parent directory
    if (strcmp(name_buf, "..") == 0) {
      current = parent->parent;
      fs_path_node_unlock(parent);

      if (current == NULL) {
        // TODO: dangling node?
        break;
      }

      fs_path_duplicate(current);

      continue;
    }

    if ((current = fs_path_lookup_cached(parent, name_buf)) != NULL) {
      fs_path_node_unlock(parent);
      continue;
    }

    parent_inode = fs_path_inode(parent);
    fs_inode_lock(parent_inode);

    r = fs_inode_lookup_locked(parent_inode, name_buf, flags, &inode);

    fs_inode_unlock(parent_inode);
    fs_inode_put(parent_inode);

    if (r == 0 && inode != NULL) {
      if ((current = fs_path_node_create(name_buf, inode, parent)) == NULL) {
        fs_inode_put(inode);
        r = -ENOMEM;
      }
    } else {
      current = NULL;
    }

    fs_path_node_unlock(parent);

    if (r < 0)
      break;

    if (*path == '\0')
      break;

    if (current == NULL) {
      r = -ENOENT;
      break;
    }
  }

  if ((r == 0) && (parent_store != NULL))
    *parent_store = parent;
  else if (parent != NULL)
    fs_path_put(parent);
  
  if ((r == 0) && (store != NULL))
    *store = current;
  else if (current != NULL)
    fs_path_put(current);

  return r;
}

int
fs_path_lookup(const char *path, char *name_buf, int flags, 
               struct PathNode **store, struct PathNode **parent_store)
{
  return fs_path_lookup_at(process_current()->cwd, path, name_buf, flags,
                           store, parent_store);
}

int
fs_lookup(const char *path, int flags, struct PathNode **pp)
{
  char name_buf[NAME_MAX + 1];

  if (strcmp(path, "/") == 0) {
    *pp = fs_path_duplicate(fs_root);
    return 0;
  }

  return fs_path_lookup(path, name_buf, flags, pp, NULL);
}

int
fs_lookup_inode(const char *path, int flags, struct Inode **inode_store)
{
  struct PathNode *path_node;
  int r;
  
  if ((r = fs_lookup(path, flags, &path_node)) < 0)
    return r;

  if (path_node == NULL)
    return -ENOENT;

  *inode_store = fs_path_inode(path_node);
  fs_path_put(path_node);

  return 0;
}

#define FS_ROOT_DEV 0
#define FS_DEV_DEV  1

void
fs_init(void)
{ 
  fs_inode_cache_init();

  fs_path_pool = k_object_pool_create("fs_path_pool",
                                      sizeof(struct PathNode),
                                      0,
                                      fs_path_node_ctor,
                                      fs_path_node_dtor);
  if (fs_path_pool == NULL)
    panic("cannot allocate fs_path_pool");

  if ((fs_root = fs_path_node_create("/", ext2_mount(FS_ROOT_DEV), NULL)) == NULL)
    panic("cannot allocate fs root");

  fs_root->parent = fs_path_duplicate(fs_root);
}

int
fs_mount(const char *type, const char *path)
{
  struct PathNode *node;
  struct Inode *root;

  if ((fs_lookup(path, 0, &node) != 0) || (node == NULL))
    return -ENOENT;

  if (!strcmp(type, "devfs")) {
    root = devfs_mount(FS_DEV_DEV);
  } else {
    fs_path_put(node);
    return -EINVAL;
  }

  // TODO: add to the list of mount points

  return fs_path_mount(node, root);
}
