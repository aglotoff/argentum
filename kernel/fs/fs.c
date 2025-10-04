#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <kernel/fs/fs.h>
#include <kernel/ipc.h>
#include <kernel/console.h>
#include <kernel/time.h>
#include <kernel/page.h>
#include <kernel/object_pool.h>
#include <kernel/process.h>
#include <kernel/dev.h>

/*
 * ----- Pathname Operations -----
 */

int
fs_access(const char *path, int amode)
{
  struct IpcMessage msg;
  struct PathNode *node;
  ino_t ino;
  struct Connection *connection;
  int r;

  if ((r = fs_path_resolve(path, 0, &node)) < 0)
    return r;
  if (node == NULL)
    return -ENOENT;

  ino = fs_path_ino(node, &connection);

  msg.type = IPC_MSG_ACCESS;
  msg.u.access.ino   = ino;
  msg.u.access.amode = amode;

  r = connection_send(connection, &msg, sizeof(msg), NULL, 0);

  fs_path_node_unref(node);

  return r;
}
 
int
fs_chdir(const char *path)
{
  struct PathNode *node;
  int r;

  if ((r = fs_path_resolve(path, 0, &node)) < 0)
    return r;
  if (node == NULL)
    return -ENOENT;

  r = fs_path_set_cwd(node);

  fs_path_node_unref(node);

  return r;
}

int
fs_chmod(const char *path, mode_t mode)
{
  struct IpcMessage msg;
  struct PathNode *node;
  ino_t ino;
  struct Connection *connection;
  int r;

  if ((r = fs_path_resolve(path, 0, &node)) < 0)
    return r;
  if (node == NULL)
    return -ENOENT;

  ino = fs_path_ino(node, &connection);

  msg.type = IPC_MSG_CHMOD;
  msg.u.chmod.ino  = ino;
  msg.u.chmod.mode = mode;

  r = connection_send(connection, &msg, sizeof(msg), NULL, 0);

  fs_path_node_unref(node);

  return r;
}

int
fs_chown(const char *path, uid_t uid, gid_t gid)
{
  struct IpcMessage msg;
  struct PathNode *node;
  ino_t ino;
  struct Connection *connection;
  int r;

  if ((r = fs_path_resolve(path, 0, &node)) < 0)
    return r;
  if (node == NULL)
    return -ENOENT;

  ino = fs_path_ino(node, &connection);

  msg.type = IPC_MSG_CHOWN;
  msg.u.chown.ino = ino;
  msg.u.chown.uid = uid;
  msg.u.chown.gid = gid;

  r = connection_send(connection, &msg, sizeof(msg), NULL, 0);

  fs_path_node_unref(node);

  return r;
}

int
fs_create(const char *path, mode_t mode, dev_t dev, struct PathNode **istore)
{
  struct IpcMessage msg;
  struct PathNode *dir;
  ino_t ino, dir_ino;
  struct Connection *chan;
  char name[NAME_MAX + 1];
  int r;

  // REF(dir)
  if ((r = fs_path_node_resolve(path, name, FS_LOOKUP_FOLLOW_LINKS, NULL, &dir)) < 0)
    return r;

  mode &= ~process_current()->cmask;

  dir_ino = fs_path_ino(dir, &chan);

  msg.type = IPC_MSG_CREATE;
  msg.u.create.dir_ino = dir_ino;
  msg.u.create.name    = name;
  msg.u.create.mode    = mode;
  msg.u.create.dev     = dev;

  r = connection_send(chan, &msg, sizeof(msg), NULL, 0);

  // (inode->ref_count): +1
  if (r > 0) {
    ino = r;
    r = 0;

    if (istore != NULL) {
      struct PathNode *pp;
      
      // REF(pp)
      if ((pp = fs_path_node_create(name, ino, chan, dir)) == NULL) {
        // fs_inode_unlock(inode);
        r = -ENOMEM;
      } else {
        *istore = pp;
      }
    } else {
      // fs_inode_unlock(inode);
      //fs_inode_put(inode);  // (inode->ref_count): -1
    }
  }

  // UNREF(dir)
  fs_path_node_unref(dir);
  dir = NULL;

  return r;
}

int
fs_link(char *path1, char *path2)
{
  struct IpcMessage msg;
  struct PathNode *dirp, *pp;
  ino_t ino, parent_ino;
  struct Connection *connection, *parent_connection;
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

  parent_ino = fs_path_ino(dirp, &parent_connection);
  ino = fs_path_ino(pp, &connection);

  k_assert(connection == parent_connection);

  msg.type = IPC_MSG_LINK;
  msg.u.link.dir_ino = parent_ino;
  msg.u.link.name    = name;
  msg.u.link.ino     = ino;

  r = connection_send(parent_connection, &msg, sizeof(msg), NULL, 0);

  // UNREF(dirp)
  fs_path_node_unref(dirp);
out1:
  // UNREF(pp)
  fs_path_node_unref(pp);
  return r;
}

ssize_t
fs_readlink(const char *path, uintptr_t va, size_t bufsize)
{
  struct IpcMessage msg;  
  struct PathNode *node;
  ino_t ino;
  struct Connection *connection;
  int r;

  if ((r = fs_path_resolve(path, 0, &node)) < 0)
    return r;
  if (node == NULL)
    return -ENOENT;

  ino = fs_path_ino(node, &connection);

  msg.type = IPC_MSG_READLINK;
  msg.u.readlink.ino   = ino;
  msg.u.readlink.nbyte = bufsize;

  r = connection_send(connection, &msg, sizeof(msg), (void *) va, bufsize);

  fs_path_node_unref(node);

  return r;
}

int
fs_rename(char *old, char *new)
{
  struct PathNode *old_dir, *new_dir, *old_node, *new_node;
  ino_t ino, parent_ino;
  struct Connection *connection, *parent_connection;
  struct IpcMessage msg;
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
    parent_ino = fs_path_ino(new_dir, &parent_connection);
    ino = fs_path_ino(new_node, &connection);

    // TODO: lock the namespace manager?
    // FIXME: check if is dir

    k_assert(parent_connection == connection);

    msg.type = IPC_MSG_UNLINK;
    msg.u.unlink.dir_ino = parent_ino;
    msg.u.unlink.ino     = ino;
    msg.u.unlink.name    = new_name;
    
    r = connection_send(parent_connection, &msg, sizeof(msg), NULL, 0);

    if (r == 0) {
      fs_path_node_remove(new_node);
    }

    // UNREF(new_node)
    fs_path_node_unref(new_node);
    new_node = NULL;

    if (r != 0)
      goto out2;
  }

  ino        = fs_path_ino(old_node, &connection);
  parent_ino = fs_path_ino(new_dir, &parent_connection);

  k_assert(parent_connection == connection);

  msg.type = IPC_MSG_LINK;
  msg.u.link.dir_ino = parent_ino;
  msg.u.link.name    = new_name;
  msg.u.link.ino     = ino;

  r = connection_send(parent_connection, &msg, sizeof(msg), NULL, 0);

  if (r < 0)
    goto out2;

  parent_ino = fs_path_ino(old_dir, &parent_connection);

  k_assert(parent_connection == connection);
    
  msg.type = IPC_MSG_UNLINK;
  msg.u.unlink.dir_ino = parent_ino;
  msg.u.unlink.ino     = ino;
  msg.u.unlink.name    = old_name;

  r = connection_send(parent_connection, &msg, sizeof(msg), NULL, 0);

  if (r == 0) {
    fs_path_node_remove(old_node);
  }

out2:
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
fs_rmdir(const char *path)
{
  struct IpcMessage msg;
  struct PathNode *dir, *pp;
  ino_t ino, parent_ino;
  struct Connection *connection, *parent_connection;
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

  parent_ino = fs_path_ino(dir, &parent_connection);
  ino = fs_path_ino(pp, &connection);

  k_assert(connection == parent_connection);

  msg.type = IPC_MSG_RMDIR;
  msg.u.rmdir.dir_ino = parent_ino;
  msg.u.rmdir.ino     = ino;
  msg.u.rmdir.name    = pp->name;

  r = connection_send(parent_connection, &msg, sizeof(msg), NULL, 0);

  if (r == 0) {
    fs_path_node_remove(pp);
  }

  // UNREF(dir)
  fs_path_node_unref(dir);

  // UNREF(pp)
  fs_path_node_unref(pp);

  return r;
}

int
fs_symlink(const char *path, const char *link_path)
{
  struct IpcMessage msg;
  struct PathNode *dir;
  ino_t dir_ino;
  struct Connection *chan;
  char name[NAME_MAX + 1];
  mode_t mode;
  int r;

  // REF(dir)
  if ((r = fs_path_node_resolve(link_path, name, FS_LOOKUP_FOLLOW_LINKS, NULL, &dir)) < 0)
    return r;

  mode = 0666 & ~process_current()->cmask;

  dir_ino = fs_path_ino(dir, &chan);

  msg.type = IPC_MSG_SYMLINK;
  msg.u.symlink.dir_ino   = dir_ino;
  msg.u.symlink.name      = name;
  msg.u.symlink.mode      = mode;
  msg.u.symlink.path      = path;

  r = connection_send(chan, &msg, sizeof(msg), NULL, 0);

  // UNREF(dir)
  fs_path_node_unref(dir);
  dir = NULL;

  return r;
}

int
fs_unlink(const char *path)
{
  struct IpcMessage msg;
  struct PathNode *dir, *pp;
  ino_t ino, parent_ino;
  struct Connection *connection, *parent_connection;
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

  parent_ino = fs_path_ino(dir, &parent_connection);
  ino        = fs_path_ino(pp, &connection);

  k_assert(parent_connection == connection);

  // TODO: lock the namespace manager?

  msg.type = IPC_MSG_UNLINK;
  msg.u.unlink.dir_ino = parent_ino;
  msg.u.unlink.ino     = ino;
  msg.u.unlink.name    = pp->name;
    
  r = connection_send(parent_connection, &msg, sizeof(msg), NULL, 0);

  if (r == 0) {
    fs_path_node_remove(pp);
  }

  // UNREF(dir)
  fs_path_node_unref(dir);

  // UNREF(pp)
  fs_path_node_unref(pp);

  return r;
}

int
fs_utime(const char *path, struct utimbuf *times)
{
  struct IpcMessage msg;
  struct PathNode *node;
  ino_t ino;
  struct Connection *connection;
  int r;

  if ((r = fs_path_resolve(path, 0, &node)) < 0)
    return r;
  if (node == NULL)
    return -ENOENT;

  ino = fs_path_ino(node, &connection);

  msg.type = IPC_MSG_UTIME;
  msg.u.utime.ino   = ino;

  if (times) {
    msg.u.utime.times = *times;
  } else {
    msg.u.utime.times.actime  = time_get_seconds();
    msg.u.utime.times.modtime = time_get_seconds();
  }

  r = connection_send(connection, &msg, sizeof(msg), NULL, 0);

  fs_path_node_unref(node);

  return r;
}

/*
 * ----- File Operations ----- 
 */

int
fs_close(struct Connection *connection)
{
  k_assert(connection->type == CONNECTION_TYPE_FILE);

  // TODO: add a comment, when node can be NULL?
  if (connection->node != NULL) {
    struct IpcMessage msg;

    msg.type = IPC_MSG_CLOSE;

    connection_send(connection, &msg, sizeof(msg), NULL, 0);

    fs_path_node_unref(connection->node);
    connection->node = NULL;
  }

  return 0;
}

#define OPEN_TIME_FLAGS (O_CREAT | O_EXCL | O_DIRECTORY | O_NOFOLLOW | O_NOCTTY | O_TRUNC)

int
fs_open(const char *path, int oflag, mode_t mode, struct Connection **file_store)
{
  struct IpcMessage msg;
  struct Connection *connection;
  struct Connection *fs_connection;
  struct PathNode *path_node;
  ino_t ino;
  int r, flags;

  // TODO: O_NONBLOCK

  //if (oflag & O_SEARCH) k_panic("O_SEARCH %s", path);
  //if (oflag & O_EXEC) k_panic("O_EXEC %s", path);
  //if (oflag & O_NOFOLLOW) k_panic("O_NOFOLLOW %s", path);
  if (oflag & O_SYNC) k_panic("O_SYNC %s", path);
  if (oflag & O_DIRECT) k_panic("O_DIRECT %s", path);

  // TODO: ENFILE
  if ((r = connection_alloc(&connection)) != 0)
    return r;

  connection->flags        = oflag & ~OPEN_TIME_FLAGS;
  connection->type         = CONNECTION_TYPE_FILE;
  connection->node         = NULL;
  connection->endpoint     = NULL;
  connection->ref_count    = 1;

  flags = FS_LOOKUP_FOLLOW_LINKS;
  if ((oflag & O_EXCL) && (oflag & O_CREAT))
    flags &= ~FS_LOOKUP_FOLLOW_LINKS;
  if (oflag & O_NOFOLLOW)
    flags &= ~FS_LOOKUP_FOLLOW_LINKS;

  // TODO: the check and the file creation should be atomic
  // REF(path_node)
  if ((r = fs_path_resolve(path, flags, &path_node)) < 0)
    goto out1;

  if (path_node == NULL) {
    if (!(oflag & O_CREAT)) {
      r = -ENOENT;
      goto out1;
    }
    
    mode &= (S_IRWXU | S_IRWXG | S_IRWXO);

    // REF(path_node)
    if ((r = fs_create(path, S_IFREG | mode, 0, &path_node)) < 0)
      goto out1;
  } else {
    if ((oflag & O_CREAT) && (oflag & O_EXCL)) {
      r = -EEXIST;
      goto out2;
    }
  }

  ino = fs_path_ino(path_node, &fs_connection);

  connection->endpoint = fs_connection->endpoint;

  msg.type = IPC_MSG_OPEN;
  msg.u.open.ino   = ino;
  msg.u.open.oflag = oflag;
  msg.u.open.mode  = mode;

  r = connection_send(connection, &msg, sizeof(msg), NULL, 0);

  if (r < 0)
    goto out2;

  // REF(connection->node)
  connection->node = fs_path_node_ref(path_node);
  
  // UNREF(path_node)
  fs_path_node_unref(path_node);
  path_node = NULL;

  *file_store = connection;

  return 0;
out2:
  // UNREF(path_node)
  fs_path_node_unref(path_node);
out1:
  connection_unref(connection);
  return r;
}

int
fs_select(struct Connection *connection, struct timeval *timeout)
{
  struct IpcMessage msg;

  k_assert(connection->ref_count > 0);
  k_assert(connection->type == CONNECTION_TYPE_FILE);

  msg.type = IPC_MSG_SELECT;
  msg.u.select.timeout = timeout;

  return connection_send(connection, &msg, sizeof(msg), NULL, 0);
}
