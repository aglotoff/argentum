#include <kernel/assert.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>

#include <kernel/cprintf.h>
#include <kernel/fs/fs.h>
#include <kernel/object_pool.h>
#include <kernel/process.h>
#include <kernel/types.h>

#include "ext2.h"

struct Inode *fs_root;

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
               struct Inode **istore, struct Inode **pstore)
{
  struct Inode *parent, *current;
  int r;

  if (*path == '\0')
    return -ENOENT;

  // For absolute paths, begin search from the root directory.
  // For relative paths, begin search from the current working directory.
  current = *path == '/'
          ? fs_inode_duplicate(fs_root)
          : fs_inode_duplicate(process_current()->cwd);
  parent  = NULL;

  while ((r = fs_path_next(path, name_buf, (char **) &path)) > 0) {
    if (parent != NULL)
      fs_inode_put(parent);
    parent = current;

    fs_inode_lock(parent);
    r = fs_inode_lookup(parent, name_buf, real, &current);
    fs_inode_unlock(parent);

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
    fs_inode_put(parent);
  
  if ((r == 0) && (istore != NULL))
    *istore = current;
  else if (current != NULL)
    fs_inode_put(current);

  return r;
}

int
fs_name_lookup(const char *path, int real, struct Inode **ip)
{
  char name_buf[NAME_MAX + 1];

  if (strcmp(path, "/") == 0) {
    *ip = fs_inode_duplicate(fs_root);
    return 0;
  }

  return fs_path_lookup(path, name_buf, real, ip, NULL);
}

#define FS_ROOT_DEV 0

void
fs_init(void)
{
  fs_inode_cache_init();
  fs_root = ext2_mount(FS_ROOT_DEV);
}
