#include <kernel/core/assert.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>

#include <kernel/console.h>
#include <kernel/fs/fs.h>
#include <kernel/object_pool.h>
#include <kernel/process.h>
#include <kernel/types.h>
#include <kernel/ipc/channel.h>

#include "devfs.h"
#include "ext2.h"

struct PathNode *fs_root;

static struct KObjectPool *fs_path_pool;
static struct KSpinLock fs_path_lock = K_SPINLOCK_INITIALIZER("fs_path");

static int              fs_path_next(const char *, char *, char **);
static void             fs_path_node_ctor(void *, size_t);
static void             fs_path_node_dtor(void *, size_t);
static int              fs_path_mount(struct PathNode *, ino_t, struct FS *);
static struct PathNode *fs_path_node_lookup_cached(struct PathNode *,
                                                   const char *);
static int              fs_path_node_lookup(struct PathNode *, const char *,
                                            int, struct PathNode **);
struct PathNode *
fs_path_node_create(const char *name,
                    ino_t ino,
                    struct Channel *channel,
                    struct PathNode *parent)
{
  struct PathNode *node;

  if ((node = (struct PathNode *) k_object_pool_get(fs_path_pool)) == NULL)
    return NULL;

  if (name != NULL)
    strncpy(node->name, name, NAME_MAX);

  k_assert(channel->u.file.inode == NULL);

  node->ino = ino;
  node->channel = channel_ref(channel);
  node->mounted_channel = NULL;
  node->mounted_ino = 0;
  node->parent = parent;
  node->ref_count++;

  if (parent != NULL) {
    k_spinlock_acquire(&fs_path_lock);
  
    parent->ref_count++;
    
    k_list_add_front(&parent->children, &node->siblings);
    node->ref_count++;

    k_spinlock_release(&fs_path_lock);
  }

  return node;
}

void
fs_path_node_remove(struct PathNode *path)
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

struct PathNode *
fs_path_node_ref(struct PathNode *node)
{
  k_assert(node != NULL);
  k_assert(node->ref_count > 0);

  k_spinlock_acquire(&fs_path_lock);
  node->ref_count++;
  k_spinlock_release(&fs_path_lock);

  return node;
}

void
fs_path_node_unref(struct PathNode *path)
{
  k_spinlock_acquire(&fs_path_lock);

  path->ref_count--;

  if ((path->ref_count == 0) && (path->parent != NULL))
    k_panic("path in bad state");

  // Move up the tree and remove all unused nodes. A node is considered unused
  // in one of two cases:
  // a) the reference count is 0
  // b) the reference count is 1, and it's referenced only by the parent node
  // TODO: parent of unmounted entry??
  while ((path != NULL) && (path->ref_count < 2)) {
    struct PathNode *parent = path->parent;

    k_assert(path->ref_count >= 0);

    if ((path->ref_count == 1) && (parent == NULL))
      // Only one link left, and this is not the parent node
      break;

    if (parent) {
      k_assert(path->ref_count == 1);
      k_list_remove(&path->siblings);
      path->ref_count--;
    }

    k_spinlock_release(&fs_path_lock);

    // TODO: ???
    if (path->mounted_channel != NULL)
      k_panic("mounted");

    if (path->channel != NULL)
      channel_unref(path->channel);

    path->ino = 0;

    k_object_pool_put(fs_path_pool, path);
    
    k_spinlock_acquire(&fs_path_lock);

    if (parent)
      parent->ref_count--;

    path = parent;
  }

  k_spinlock_release(&fs_path_lock);
}

ino_t
fs_path_ino(struct PathNode *node, struct Channel **channelp)
{
  ino_t ino;
  struct Channel *channel;

  k_spinlock_acquire(&fs_path_lock);
  ino = node->mounted_ino ? node->mounted_ino : node->ino;
  channel = node->mounted_channel ? node->mounted_channel : node->channel;
  k_spinlock_release(&fs_path_lock);

  if (channelp)
    *channelp = channel;

  return ino;
}

void
fs_path_node_lock(struct PathNode *node)
{
  k_assert((node->ref_count != 1) || (node->parent == NULL));

  if (k_mutex_lock(&node->mutex) < 0)
    k_panic("lock");
}

void
fs_path_node_unlock(struct PathNode *node)
{
  k_assert((node->ref_count != 1) || (node->parent == NULL));

  if (k_mutex_unlock(&node->mutex) < 0)
    k_panic("unlock");
}

static struct PathNode *
fs_path_parent_ref(struct PathNode *node)
{
  struct PathNode *parent;

  fs_path_node_lock(node);
  parent = node->parent ? fs_path_node_ref(node->parent) : NULL;
  fs_path_node_unlock(node);

  return parent;
}

int
fs_path_set_cwd(struct PathNode *node)
{
  struct Process *current = process_current();
  struct FSMessage msg;
  ino_t ino;
  struct Channel *channel;
  int r;

  ino = fs_path_ino(node, &channel);

  msg.type = FS_MSG_CHDIR;
  msg.u.chdir.ino = ino;

  fs_send_recv(channel, &msg);

  if ((r = msg.u.chdir.r) < 0)
    return r;

  // UNREF(current->cwd)
  fs_path_node_unref(current->cwd);
  current->cwd = NULL;

  // REF(current->cwd)
  current->cwd = fs_path_node_ref(node);

  return r;
}

static ssize_t
fs_path_resolve_symlink(ino_t ino,
                        struct Channel *channel,
                        const char *rest,
                        char **path_store)
{
  struct FSMessage msg;  
  char *path;
  ssize_t r;

  if ((path = (char *) k_malloc(PATH_MAX)) == NULL)
    return -ENOMEM;

  msg.type = FS_MSG_READLINK;
  msg.u.readlink.ino   = ino;
  msg.u.readlink.va    = (uintptr_t) path;
  msg.u.readlink.nbyte = PATH_MAX - 1;

  fs_send_recv(channel, &msg);

  if ((r = msg.u.readlink.r) < 0) {
    k_free(path);
    return r;
  }

  // FIXME: what if r is 0?
  k_assert((r > 0) && (r < PATH_MAX));

  if ((path[r - 1] != '/') && (*rest != '\0'))
    path[r++] = '/';

  strncpy(&path[r], rest, PATH_MAX - 1 - r);
  path[PATH_MAX - 1] = '\0';

  if (path_store != NULL)
    *path_store = path;

  return r;
}

int
fs_path_node_resolve_at(struct PathNode *start,
                   const char *path,
                   char *name_buf,
                   int flags,
                   struct PathNode **store,
                   struct PathNode **parent_store)
{
  struct PathNode *prev, *next;
  char *p;
  ssize_t r;
  int symlink_count = 0;

  if (*path == '\0')
    return -ENOENT;

  if ((p = (char *) k_malloc(PATH_MAX)) == NULL)
    return -ENOMEM;

  strncpy(p, path, PATH_MAX - 1);
  p[PATH_MAX - 1] = '\0';

  // For absolute paths, begin search from the root directory.
  // For relative paths, begin search from the specifed starting directory.
  next = fs_path_node_ref(*p == '/' ? fs_root : start);
  prev = NULL;

  while ((r = fs_path_next(p, name_buf, (char **) &p)) > 0) {
    struct FSMessage msg;
    struct stat stat;
    ino_t ino;
    struct Channel *channel;
    char *resolved_path;

    // Stay in the current directory
    if (strcmp(name_buf, ".") == 0)
      continue;

    // Drop reference to the previous node
    if (prev != NULL) {
      // UNREF(prev)
      fs_path_node_unref(prev);
      prev = NULL;
    }

    prev = next;
    next = NULL;

    // Move to the parent directory
    if (strcmp(name_buf, "..") == 0) {
      // REF(next)
      next = fs_path_parent_ref(prev);

      if (next == NULL) {
        // TODO: how could this happen?
        k_panic("dangling node");
      }

      continue;
    }

    // REF(next)
    r = fs_path_node_lookup(prev, name_buf, flags, &next);

    if (r < 0)
      break;

    if (next == NULL) {
      if (*p != '\0') {
        r = -ENOENT;
      }
      break;
    }

    // Check for symlinks

    ino = fs_path_ino(next, &channel);

    msg.type = FS_MSG_STAT;
    msg.u.stat.ino = ino;
    msg.u.stat.buf = &stat;

    fs_send_recv(channel, &msg);

    if ((r = msg.u.stat.r) < 0) {
      break;
    }

    if (!S_ISLNK(stat.st_mode)) {
      continue;
    }

    if ((*p == '\0') && !(flags & FS_LOOKUP_FOLLOW_LINKS)) {
      break;
    }

    // FIXME: symlinks and slashes

    // FIXME: must be a defined constant
    if (++symlink_count > 20) {
      r = -ELOOP;
      break;
    }

    // Resolve the symlink

    if ((r = fs_path_resolve_symlink(ino, channel, p, &resolved_path)) < 0) {
      break;
    }

    k_free(p);
    p = resolved_path;

    // UNREF(next)
    fs_path_node_unref(next);
    next = NULL;

    // Absolute symlink
    if (*p == '/') {
      // REF(next)
      next = fs_path_node_ref(fs_root);
      continue;
    }

    // Relative symlink
    // REF(next)
    next = fs_path_node_ref(prev);
  }

  // FIXME: prev could be not a parent node if we looked up ".."
  if ((r == 0) && (parent_store != NULL))
    *parent_store = prev;
  else if (prev != NULL)
    fs_path_node_unref(prev);
  
  if ((r == 0) && (store != NULL))
    *store = next;
  else if (next != NULL)
    fs_path_node_unref(next);

  k_free(p);

  return r;
}

static int
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

static int
fs_path_node_lookup(struct PathNode *parent,
                    const char *name,
                    int flags,
                    struct PathNode **child_store)
{
  struct FSMessage msg;
  struct PathNode *child;
  int r = 0;
  
  fs_path_node_lock(parent);

  // REF(child)
  child = fs_path_node_lookup_cached(parent, name);

  if (child == NULL) {
    ino_t parent_ino, child_ino;
    struct Channel *channel;

    parent_ino = fs_path_ino(parent, &channel);

    msg.type = FS_MSG_LOOKUP;
    msg.u.lookup.dir_ino = parent_ino;
    msg.u.lookup.name    = name;
    msg.u.lookup.flags   = flags;
    msg.u.lookup.istore  = &child_ino;

    fs_send_recv(channel, &msg);

    r = msg.u.lookup.r;

    if (r == 0 && child_ino != 0) {
      // Insert new node
      if ((child = fs_path_node_create(name, child_ino, channel, parent)) == NULL) {
        r = -ENOMEM;
      }
    } else {
      child = NULL;
    }
  }

  fs_path_node_unlock(parent);

  if (child_store != NULL) {
    *child_store = child;
  } else if (child != NULL) {
    // UNREF(child)
    fs_path_node_unref(child);
  }

  return r;
}

static struct PathNode *
fs_path_node_lookup_cached(struct PathNode *parent, const char *name)
{
  struct KListLink *l;

  // TODO: this is a bottleneck! 
  // if there are many child nodes, comparing all names may take too long, and
  // we're blocking the entire system. Could we use a per-node mutex instead?
  // or a hash table?

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
fs_path_node_resolve(const char *path,
                     char *name_buf,
                     int flags, 
                     struct PathNode **store,
                     struct PathNode **parent_store)
{
  return fs_path_node_resolve_at(process_current()->cwd,
                                 path,
                                 name_buf,
                                 flags,
                                 store,
                                 parent_store);
}

int
fs_path_resolve(const char *path, int flags, struct PathNode **store)
{
  char name_buf[NAME_MAX + 1];

  if (strcmp(path, "/") == 0) {
    // REF(store)
    *store = fs_path_node_ref(fs_root);
    return 0;
  }

  return fs_path_node_resolve(path, name_buf, flags, store, NULL);
}

#define FS_ROOT_DEV 0
#define FS_DEV_DEV  1

void
fs_init(void)
{ 
  ino_t root_ino;
  struct Channel *channel;

  fs_inode_cache_init();

  fs_path_pool = k_object_pool_create("fs_path_pool",
                                      sizeof(struct PathNode),
                                      0,
                                      fs_path_node_ctor,
                                      fs_path_node_dtor);
  if (fs_path_pool == NULL)
    k_panic("cannot allocate fs_path_pool");

  if (channel_alloc(&channel) != 0)
    k_panic("cannot allocate channel");

  channel->flags        = 0;
  channel->type         = CHANNEL_TYPE_FILE;
  channel->u.file.node  = NULL;
  channel->u.file.inode = NULL;
  channel->u.file.rdev  = -1;
  channel->ref_count    = 1;

  root_ino = ext2_mount(FS_ROOT_DEV, &channel->u.file.fs);

  if ((fs_root = fs_path_node_create("/", root_ino, channel, NULL)) == NULL)
    k_panic("cannot allocate fs root");

  fs_root->parent = fs_path_node_ref(fs_root);
}

static void
fs_path_node_ctor(void *ptr, size_t)
{
  struct PathNode *node = (struct PathNode *) ptr;

  node->mounted_channel = NULL;
  node->mounted_ino = 0;
  node->ref_count = 0;
  k_list_init(&node->children);
  k_list_null(&node->siblings);
  k_mutex_init(&node->mutex, "path_node");
}

static void
fs_path_node_dtor(void *ptr, size_t)
{
  struct PathNode *node = (struct PathNode *) ptr;

  k_assert(node->mounted_channel == NULL);
  k_assert(node->ref_count == 0);
  k_assert(k_list_is_empty(&node->children));
  k_assert(k_list_is_empty(&node->siblings));
  k_assert(!k_mutex_holding(&node->mutex));
}

int
fs_mount(const char *type, const char *path)
{
  struct PathNode *node;
  ino_t root_ino;
  struct FS *fs;

  // REF(node)
  if ((fs_path_resolve(path, 0, &node) != 0) || (node == NULL))
    return -ENOENT;

  if (strcmp(type, "devfs") == 0) {
    root_ino = devfs_mount(FS_DEV_DEV, &fs);
  } else {
    return -EINVAL;
  }

  // TODO: add to the list of mount points

  return fs_path_mount(node, root_ino, fs);
}

static int
fs_path_mount(struct PathNode *node, ino_t ino, struct FS *fs)
{
  struct Channel *channel;
  int r;

  // TODO: support multiple mountpoints
  // TODO: lock

  if (node->mounted_channel)
    k_panic("already mounted");

  if ((r = channel_alloc(&channel)) < 0)
    return r;

  channel->flags        = 0;
  channel->type         = CHANNEL_TYPE_FILE;
  channel->u.file.node  = NULL;
  channel->u.file.inode = NULL;
  channel->u.file.fs    = fs;
  channel->u.file.rdev  = -1;
  channel->ref_count    = 1;

  node->mounted_ino     = ino;
  node->mounted_channel = channel;

  // TODO: remove cached entries?

  return 0;
}
