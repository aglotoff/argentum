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

static struct ObjectPool *path_pool;
static struct SpinLock path_lock = SPIN_INITIALIZER("path");

struct PathNode *
fs_path_create(const char *name)
{
  struct PathNode *path;

  if ((path = (struct PathNode *) object_pool_get(path_pool)) == NULL)
    return NULL;

  if (name != NULL)
    strncpy(path->name, name, NAME_MAX);
  path->inode = NULL;
  path->ref_count = 1;
  path->parent = NULL;
  list_init(&path->children);
  list_init(&path->siblings);
  kmutex_init(&path->mutex, "path");

  return path;
}

struct PathNode *
fs_path_duplicate(struct PathNode *path)
{
  spin_lock(&path_lock);
  path->ref_count++;
  spin_unlock(&path_lock);

  // cprintf("[%s: %d]\n", path->name, path->ref_count);

  return path;
}

void
fs_path_put(struct PathNode *path)
{
  int count;

  spin_lock(&path_lock);
  count = --path->ref_count;
  spin_unlock(&path_lock);

  if (count < 0)
    panic("negative count for path %s\n", path->name);

  // TODO: go up the tree and remove all unused nodes

  if (count == 0) {
    if (path->inode != NULL)
      fs_inode_put(path->inode);

    object_pool_put(path_pool, path);
  }
}

void
fs_path_lock(struct PathNode *node)
{
  kmutex_lock(&node->mutex);
}

void
fs_path_unlock(struct PathNode *node)
{
  kmutex_unlock(&node->mutex);
}

void
fs_path_lock_two(struct PathNode *node1, struct PathNode *node2)
{
  if (node1 < node2) {
    fs_path_lock(node1);
    fs_path_lock(node2);
  } else {
    fs_path_lock(node2);
    fs_path_lock(node1);
  }
}

void
fs_path_unlock_two(struct PathNode *node1, struct PathNode *node2)
{
  if (node1 < node2) {
    fs_path_unlock(node2);
    fs_path_unlock(node1);
  } else {
    fs_path_unlock(node1);
    fs_path_unlock(node2);
  }
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

int
fs_path_lookup(const char *path, char *name_buf, int real, 
               struct PathNode **istore, struct PathNode **pstore)
{
  struct PathNode *parent, *current;
  int r;
  struct ListLink *l;

  if (*path == '\0')
    return -ENOENT;

  // For absolute paths, begin search from the root directory.
  // For relative paths, begin search from the current working directory.
  current = fs_path_duplicate(*path == '/' ? fs_root : process_current()->cwd);
  parent  = NULL;

  while ((r = fs_path_next(path, name_buf, (char **) &path)) > 0) {
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
      current = fs_path_duplicate(parent->parent);
      fs_path_unlock(parent);
      continue;
    }

    LIST_FOREACH(&parent->children, l) {
      struct PathNode *p = LIST_CONTAINER(l, struct PathNode, siblings);
      
      if (strcmp(p->name, name_buf) == 0) {
        current = fs_path_duplicate(p);
        break;
      }
    }

    if (current != NULL) {
      fs_path_unlock(parent);
      continue;
    }

    if ((current = fs_path_create(name_buf)) == NULL) {
      r = -ENOMEM;
      break;
    }

    r = fs_inode_lookup(parent->inode, name_buf, real, &current->inode);

    if (r == 0 && current->inode != NULL) {
      current->parent = fs_path_duplicate(parent);
      list_add_front(&parent->children, &current->siblings);
      fs_path_duplicate(current);
    } else {
      fs_path_put(current);
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
fs_name_lookup(const char *path, int real, struct PathNode **pp)
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

  path_pool = object_pool_create("path_pool", sizeof(struct PathNode), 0,
                                 NULL, NULL);
  if (path_pool == NULL)
    panic("cannot allocate path_pool");

  if ((fs_root = fs_path_create("/")) == NULL)
    panic("cannot allocate fs root");

  fs_root->inode  = ext2_mount(FS_ROOT_DEV);
  fs_root->parent = fs_path_duplicate(fs_root);
}

int
fs_access(const char *path, int amode)
{
  struct PathNode *pp;
  int r;

  if ((r = fs_name_lookup(path, 0, &pp)) < 0)
    return r;

  if (pp == NULL)
    return -ENOENT;

  r = amode != F_OK ? fs_inode_access(pp->inode, amode) : 0;

  fs_path_put(pp);

  return r;
}
