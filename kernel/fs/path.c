#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>

#include <argentum/cprintf.h>
#include <argentum/fs/fs.h>
#include <argentum/mm/kmem.h>
#include <argentum/process.h>
#include <argentum/types.h>

#include "ext2.h"

struct Inode *fs_root;

static size_t
fs_path_next(const char *path, char *name_buf, char **result)
{
  const char *end;
  ptrdiff_t n;

  // Skip leading slashes
  while (*path == '/')
    path++;
  
  // This was the last path segment
  if (*path == '\0')
    return 0;

  // Find the end of the path segment
  for (end = path; (*end != '\0') && (*end != '/'); end++)
    ;

  n = end - path;
  strncpy(name_buf, path, MIN(n, NAME_MAX));
  name_buf[n] = '\0';

  // Skip trailing slashes
  while (*end == '/')
    end++;

  if (result)
    *result = (char *) end;

  return n;
}

int
fs_path_lookup(const char *path, char *name_buf, int parent,
               struct Inode **istore)
{
  struct Inode *current, *next;
  size_t name_len;

  if (*path == '\0')
    return -ENOENT;

  if (strnlen(path, PATH_MAX) >= PATH_MAX)
    return -ENAMETOOLONG;

  // For absolute paths, begin search from the root directory.
  // For relative paths, begin search from the current working directory.
  current = *path == '/'
          ? fs_inode_duplicate(fs_root)
          : fs_inode_duplicate(process_current()->cwd);

  while ((name_len = fs_path_next(path, name_buf, (char **) &path)) > 0) {
    if (name_len > NAME_MAX) {
      fs_inode_put(current);
      return -ENAMETOOLONG;
    }

    fs_inode_lock(current);

    // Check that the current inode is a directory and we have permissions to
    // search inside
    if (!S_ISDIR(current->mode)) {
      fs_inode_unlock_put(current);
      return -ENOTDIR;
    }
    if (!fs_inode_can_execute(current)) {
      fs_inode_unlock_put(current);
      return -EACCESS;
    }

    // Found the parent node
    if (parent && (*path == '\0')) {
      fs_inode_unlock(current);

      if (istore != NULL)
        *istore = current;

      return 0;
    }

    // Move one level down
    next = ext2_inode_lookup(current, name_buf);

    fs_inode_unlock_put(current);

    if (next == NULL)
      return -ENOENT;

    current = next;
  }

  if (parent) {
    // File exists!
    fs_inode_put(current);
    return -EEXISTS;
  }

  if (istore)
    *istore = current;

  return 0;
}

int
fs_name_lookup(const char *path, struct Inode **ip)
{
  char name_buf[NAME_MAX + 1];

  if (strcmp(path, "/") == 0) {
    *ip = fs_inode_duplicate(fs_root);
    return 0;
  }

  return fs_path_lookup(path, name_buf, 0, ip);
}

void
fs_init(void)
{
  fs_inode_cache_init();
  fs_root = ext2_mount();
}
