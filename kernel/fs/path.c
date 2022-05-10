#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>

#include <kernel/cprintf.h>
#include <kernel/fs/ext2.h>
#include <kernel/mm/kobject.h>
#include <kernel/process.h>
#include <kernel/types.h>

#include <kernel/fs/fs.h>

static size_t
fs_path_skip(const char *path, char *name, char **next)
{
  const char *end;
  ptrdiff_t n;

  // Skip leading slashes
  while (*path == '/')
    path++;
  
  // This was the last element
  if (*path == '\0')
    return 0;

  // Find the end of the element
  for (end = path; (*end != '\0') && (*end != '/'); end++)
    ;

  n = end - path;
  strncpy(name, path, MIN(n, NAME_MAX));
  name[n] = '\0';

  while (*end == '/')
    end++;

  if (next)
    *next = (char *) end;

  return n;
}

int
fs_path_lookup(const char *path, char *name, int parent, struct Inode **istore)
{
  struct Inode *ip, *next;
  size_t namelen;

  // For absolute paths, begin search from the root directory.
  // For relative paths, begin search from the current working directory.
  ip = *path == '/' ? fs_inode_get(2, 0) : fs_inode_dup(my_process()->cwd);

  while ((namelen = fs_path_skip(path, name, (char **) &path)) > 0) {
    if (namelen > NAME_MAX) {
      fs_inode_put(ip);
      return -ENAMETOOLONG;
    }

    fs_inode_lock(ip);

    if ((ip->mode & EXT2_S_IFMASK) != EXT2_S_IFDIR) {
      fs_inode_unlock_put(ip);
      return -ENOTDIR;
    }

    if (parent && (*path == '\0')) {
      fs_inode_unlock(ip);

      if (istore)
        *istore = ip;
      return 0;
    }

    if ((next = ext2_inode_lookup(ip, name)) == NULL) {
      fs_inode_unlock_put(ip);
      return -ENOENT;
    }

    fs_inode_unlock_put(ip);

    ip = next;
  }

  if (parent) {
    // File exists!
    fs_inode_put(ip);
    return -EEXISTS;
  }

  if (istore)
    *istore = ip;

  return 0;
}

int
fs_name_lookup(const char *path, struct Inode **ip)
{
  char name[NAME_MAX + 1];

  if (strcmp(path, "/") == 0) {
    *ip = fs_inode_get(2, 0);
    return 0;
  }

  return fs_path_lookup(path, name, 0, ip);
}

struct Inode **fs_root;

void
fs_init(void)
{
  fs_inode_cache_init();
  ext2_read_superblock();
}
