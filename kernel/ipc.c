#include <sys/types.h>
#include <kernel/console.h>
#include <kernel/core/spinlock.h>
#include <kernel/core/assert.h>
#include <kernel/ipc.h>
#include <kernel/object_pool.h>
#include <kernel/hash.h>

struct Channel {
  id_t             id;

  // Access protected by channel_id.lock
  struct KListLink id_hash_link;

  // Access protected by ipc_lock
  int              ref_count;
  int              active;
  struct KListLink connections;
};

struct Connection {
  id_t             id;

  // Access protected by ipc_lock
  struct Channel  *channel;
  int              ref_count;
  struct KListLink link;
};

static struct KSpinLock ipc_lock = K_SPINLOCK_INITIALIZER("ipc");

#define CHANNEL_ID_HASH_SIZE  32

static struct KObjectPool *channel_pool;

static struct {
  struct KListLink table[CHANNEL_ID_HASH_SIZE];
  id_t             next;
  struct KSpinLock lock;
} channel_id;

static struct KObjectPool *connection_pool;

#define CONNECTION_MAX  64

static struct {
  struct Connection *table[CONNECTION_MAX];
  struct KSpinLock   lock;
} connection_desc;

static void
ipc_channel_ctor(void *p, size_t)
{
  struct Channel *channel = (struct Channel *) p;
  k_list_null(&channel->id_hash_link);
  k_list_init(&channel->connections);
  channel->active = 0;
}

static void
ipc_channel_dtor(void *p, size_t)
{
  struct Channel *channel = (struct Channel *) p;
  k_assert(k_list_is_null(&channel->id_hash_link));
  k_assert(k_list_is_empty(&channel->connections));
  k_assert(!channel->active);
}

int
ipc_channel_create(struct Channel **channel_store)
{
  struct Channel *channel;
  id_t id;

  if ((channel = (struct Channel *) k_object_pool_get(channel_pool)) == NULL)
    k_panic("cannot create channel");

  channel->active = 1;
  channel->ref_count = 1;

  k_spinlock_acquire(&channel_id.lock);

  if ((channel->id = id = ++channel_id.next) < 0)
    k_panic("channel id overflow");

  HASH_PUT(channel_id.table, &channel->id_hash_link, channel->id);
  
  k_spinlock_acquire(&ipc_lock);

  if (channel_store != NULL) {
    channel->ref_count++;
    *channel_store = channel;
  }

  k_spinlock_release(&ipc_lock);

  k_spinlock_release(&channel_id.lock);

  return id;
}

struct Channel *
ipc_channel_dup(struct Channel *channel)
{
  k_spinlock_acquire(&ipc_lock);
  channel->ref_count++;
  k_spinlock_release(&ipc_lock);

  return channel;
}

struct Channel *
ipc_channel_get(id_t id)
{
  struct KListLink *l;
  struct Channel *channel;

  k_spinlock_acquire(&channel_id.lock);

  HASH_FOREACH_ENTRY(channel_id.table, l, id) {
    channel = KLIST_CONTAINER(l, struct Channel, id_hash_link);
    if (channel->id == id) {
      channel = ipc_channel_dup(channel);
      k_spinlock_release(&channel_id.lock);

      return channel;
    }
  }

  k_spinlock_release(&channel_id.lock);

  return NULL;
}

void
ipc_channel_put(struct Channel *channel)
{
  int ref_remain;

  cprintf("put channel %d\n", channel->id);
  
  k_spinlock_acquire(&ipc_lock);

  ref_remain = --channel->ref_count;
  
  k_spinlock_release(&ipc_lock);

  if (ref_remain == 0) {
    k_assert(!channel->active);

    cprintf("freed channel %d\n", channel->id);

    ipc_channel_dtor(channel, sizeof *channel);
    k_object_pool_put(channel_pool, channel);
  }
}

void
ipc_channel_destroy(struct Channel *channel)
{
  struct KListLink *link;
  
  k_spinlock_acquire(&channel_id.lock);
  k_list_remove(&channel->id_hash_link);
  k_spinlock_release(&channel_id.lock);

  k_spinlock_acquire(&ipc_lock);

  channel->active = 0;

  // Shutdown all connections
  KLIST_FOREACH(&channel->connections, link) {
    struct Connection *conn = KLIST_CONTAINER(link, struct Connection, link);

    k_list_remove(&conn->link);
    conn->channel = NULL;

    // TODO: shutdown
  }

  k_spinlock_release(&ipc_lock);
  
  ipc_channel_put(channel);
}

int
ipc_connect_attach(struct Channel *channel, struct Connection **conn_store)
{
  struct Connection *connection;
  id_t id;

  connection = (struct Connection *) k_object_pool_get(connection_pool);
  if (connection == NULL)
    k_panic("cannot create connection");

  connection->ref_count = 1;

  k_list_null(&connection->link);

  k_spinlock_acquire(&ipc_lock);

  if (!channel->active)
    k_panic("channel destroyed");

  connection->channel = ipc_channel_dup(channel);
  k_list_add_back(&channel->connections, &connection->link);

  k_spinlock_release(&ipc_lock);

  k_spinlock_acquire(&connection_desc.lock);

  for (id = 0; id < CONNECTION_MAX; id++)
    if (connection_desc.table[id] == NULL)
      break;

  if (id == CONNECTION_MAX)
    k_panic("cannot allocate a connection descriptor");

  connection_desc.table[id] = connection;

  k_spinlock_acquire(&ipc_lock);
  connection->ref_count++;

  if (conn_store != NULL) {
    connection->ref_count++;
    *conn_store = connection;
  }

  k_spinlock_release(&ipc_lock);

  k_spinlock_release(&connection_desc.lock);

  return id;
}

void
ipc_init(void)
{
  channel_pool = k_object_pool_create("channel_pool",
                                      sizeof(struct Channel),
                                      0,
                                      ipc_channel_ctor,
                                      ipc_channel_dtor);
  if (channel_pool == NULL)
    k_panic("cannot create channel_pool");

  connection_pool = k_object_pool_create("connection_pool",
                                         sizeof(struct Connection),
                                         0,
                                         NULL,
                                         NULL);
  if (connection_pool == NULL)
    k_panic("cannot create connection_pool");

  HASH_INIT(channel_id.table);
  k_spinlock_init(&channel_id.lock, "channel_id");

  k_spinlock_init(&connection_desc.lock, "connection_desc");

  struct Channel *chan;

  cprintf("IPC! %d %d\n", ipc_channel_create(&chan));

  ipc_channel_destroy(chan);
  ipc_channel_put(chan);

}
