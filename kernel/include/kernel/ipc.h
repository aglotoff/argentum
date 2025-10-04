#ifndef __KERNEL_INCLUDE_KERNEL_IPC_CONNECTION_H__
#define __KERNEL_INCLUDE_KERNEL_IPC_CONNECTION_H__

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/ipc.h>

#include <kernel/core/semaphore.h>
#include <kernel/core/mailbox.h>

struct Inode;
struct stat;

enum {
  CONNECTION_TYPE_FILE = 1,
  CONNECTION_TYPE_PIPE = 2,
  CONNECTION_TYPE_SOCKET = 3,
};

struct File {
  struct KListLink hash_link;
  struct Connection  *connection;
  off_t            offset;       // Current offset within the file
  struct Inode    *inode;
  dev_t            rdev;
};

struct Endpoint;

struct Connection {
  int                  type;         // File type (inode, console, or pipe)
  int                  ref_count;    // The number of references to this file
  
  int                  flags;
  struct PathNode     *node;         // Pointer to the corresponding inode
  
  struct Endpoint     *endpoint;
};

int                connection_alloc(struct Connection **);
void               connection_init(void);
struct Connection *connection_ref(struct Connection *);
void               connection_unref(struct Connection *);
int                connection_get_flags(struct Connection *);
int                connection_set_flags(struct Connection *, int);

ssize_t         connection_read(struct Connection *, uintptr_t, size_t);
int             connection_stat(struct Connection *, struct stat *);
int             connection_chdir(struct Connection *);
off_t           connection_seek(struct Connection *, off_t, int);
int             connection_chown(struct Connection *, uid_t, gid_t);
int             connection_ioctl(struct Connection *, int, int);
int             connection_select(struct Connection *, struct timeval *);
intptr_t        connection_send(struct Connection *, void *, size_t, void *, size_t);
intptr_t        connection_sendv(struct Connection *, struct iovec *, int, struct iovec *, int);

struct Request {
  struct iovec *send_iov;
  int           send_iov_cnt;
  int           send_idx;
  size_t        send_pos;

  struct iovec *recv_iov;
  int           recv_iov_cnt;
  int           recv_idx;
  size_t        recv_pos;

  struct KSemaphore sem;
  struct Process *process;
  struct Connection *connection;
  struct KSpinLock lock;
  int ref_count;

  intptr_t r;
};

struct Request *request_create(void);
void            request_destroy(struct Request *);
void            request_dup(struct Request *);
void            request_reply(struct Request *, intptr_t);
ssize_t         request_write(struct Request *, void *, size_t);
ssize_t         request_read(struct Request *, void *, size_t);

#define ENDPOINT_MBOX_CAPACITY  20

struct Endpoint {
  struct KMailBox mbox;
  uint8_t         mbox_buf[ENDPOINT_MBOX_CAPACITY * sizeof(void *)];
};

void endpoint_init(struct Endpoint *);
int  endpoint_receive(struct Endpoint *, struct Request **);

#endif  // !__KERNEL_INCLUDE_KERNEL_IPC_CONNECTION__
