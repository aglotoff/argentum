#include <kernel/console.h>
#include <kernel/ipc/channel.h>
#include <kernel/page.h>
#include <kernel/net.h>
#include <kernel/core/task.h>
#include <kernel/object_pool.h>
#include <kernel/vmspace.h>
#include <kernel/process.h>
#include <kernel/hash.h>
#include <netdb.h>

#include <lwip/api.h>
#include <lwip/dhcp.h>
#include <lwip/etharp.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>
#include <lwip/tcpip.h>
#include <lwip/icmp.h>
#include <lwip/inet_chksum.h>

void arch_eth_write(const void *, size_t);

static struct netif eth_netif;

void
net_enqueue(void *data, size_t count)
{
  struct pbuf *p = pbuf_alloc(PBUF_RAW, count, PBUF_POOL);

  if (p != NULL) {
    pbuf_take(p, data, count);

    if (eth_netif.input(p, &eth_netif) != 0)
      pbuf_free(p);
  }
}

static err_t
eth_netif_output(struct netif *netif, struct pbuf *p)
{
  (void) netif;

  struct Page *page = page_alloc_one(0, PAGE_TAG_ETH_TX);

  pbuf_copy_partial(p, page2kva(page), p->tot_len, 0);
  arch_eth_write(page2kva(page), p->tot_len);

  page_free_one(page);

  return ERR_OK;
}

// FIXME: temporary global var
uint8_t mac_addr[6];

static err_t 
eth_netif_init(struct netif *netif)
{
  netif->linkoutput = eth_netif_output;
  netif->output     = etharp_output;
  netif->mtu        = 1500;
  netif->flags      = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP |
                      NETIF_FLAG_ETHERNET | NETIF_FLAG_IGMP | NETIF_FLAG_MLD6 |
                      NETIF_FLAG_LINK_UP;
  netif->hwaddr_len = ETH_HWADDR_LEN;
  netif->hwaddr[0]  = mac_addr[0];
  netif->hwaddr[1]  = mac_addr[1];
  netif->hwaddr[2]  = mac_addr[2];
  netif->hwaddr[3]  = mac_addr[3];
  netif->hwaddr[4]  = mac_addr[4];
  netif->hwaddr[5]  = mac_addr[5];

  return ERR_OK;
}

static void
net_init_done(void *arg)
{
  (void) arg;

  ip4_addr_t addr;
  ip4_addr_t netmask;
  ip4_addr_t gw;

  IP4_ADDR(&addr, 10, 0, 2, 15);
  IP4_ADDR(&netmask, 255, 255, 0, 0);
  IP4_ADDR(&gw, 10, 0, 2, 2);

  netif_add(&eth_netif, &addr, &netmask, &gw, NULL, eth_netif_init, tcpip_input);

  eth_netif.name[0] = 'e';
  eth_netif.name[1] = '0';
  netif_set_default(&eth_netif);
  netif_set_up(&eth_netif);

  dhcp_start(&eth_netif);
}

struct SocketEndpoint {
  struct KListLink hash_link;
  struct Channel  *channel;
  int              socket;
};

#define NBUCKET   256

static struct {
  struct KListLink table[NBUCKET];
  struct KSpinLock lock;
} socket_hash;

static struct SocketEndpoint *
net_get_channel_endpoint(struct Channel *channel)
{
  struct KListLink *l;

  k_spinlock_acquire(&socket_hash.lock);

  HASH_FOREACH_ENTRY(socket_hash.table, l, (uintptr_t) channel) {
    struct SocketEndpoint *endpoint;
    
    endpoint = KLIST_CONTAINER(l, struct SocketEndpoint, hash_link);
    if (endpoint->channel == channel) {
      k_spinlock_release(&socket_hash.lock);
      return endpoint;
    }
  }

  k_spinlock_release(&socket_hash.lock);

  k_warn("socket endpoint not found");

  return NULL;
}

static void
net_set_channel_endpoint(struct SocketEndpoint *endpoint,
                         struct Channel *channel,
                         int socket)
{
  endpoint->channel = channel;
  endpoint->socket  = socket;
  k_list_null(&endpoint->hash_link);

  channel->type = CHANNEL_TYPE_SOCKET;
  channel->ref_count++;

  k_spinlock_acquire(&socket_hash.lock);
  HASH_PUT(socket_hash.table, &endpoint->hash_link, (uintptr_t) channel);
  k_spinlock_release(&socket_hash.lock);
}

void
net_init(void)
{
  HASH_INIT(socket_hash.table);
  k_spinlock_init(&socket_hash.lock, "socket_hash");

  tcpip_init(net_init_done, NULL);
}

int
net_send_recv(struct Channel *channel, struct IpcMessage *msg)
{
  switch (msg->type) {
  case IPC_MSG_CLOSE:
    msg->r = net_close(channel);
    break;
  case IPC_MSG_SEEK:
    msg->r = -ESPIPE;
    break;
  case IPC_MSG_FCHMOD:
    msg->r = -EBADF;
    break;
  case IPC_MSG_READ:
    msg->r = net_read(channel, msg->u.read.va, msg->u.read.nbyte);
    break;
  case IPC_MSG_WRITE:
    msg->r = net_write(channel, msg->u.write.va, msg->u.write.nbyte);
    break;
  case IPC_MSG_READDIR:
    msg->r = -ENOTDIR;
    break;
  case IPC_MSG_FSTAT:
    msg->r = -EBADF;
    break;
  case IPC_MSG_FCHOWN:
    msg->r = -EBADF;
    break;
  case IPC_MSG_IOCTL:
    msg->r = -EBADF;
    break;
  case IPC_MSG_TRUNC:
    msg->r = -EBADF;
    break;
  case IPC_MSG_FSYNC:
    msg->r = -EBADF;
    break;
  default:
    return -ENOSYS;
  }

  return 0;
}

int
net_socket(int domain, int type, int protocol, struct Channel **fstore)
{
  struct Channel *f;
  struct SocketEndpoint *endpoint;
  int r, socket;

  if ((endpoint = k_malloc(sizeof *endpoint)) == NULL)
    return -ENOMEM;

  if ((socket = lwip_socket(domain, type, protocol)) < 0) {
    k_free(endpoint);
    return -errno;
  }

  if ((r = channel_alloc(&f)) < 0) {
    lwip_close(socket);
    k_free(endpoint);
    return r;
  }

  net_set_channel_endpoint(endpoint, f, socket);

  if (fstore != NULL)
    *fstore = f;
  
  return 0;
}

int
net_bind(struct Channel *file, const struct sockaddr *address, socklen_t address_len)
{
  struct SocketEndpoint *endpoint = net_get_channel_endpoint(file);

  if (lwip_bind(endpoint->socket, address, address_len) != 0)
    return -errno;

  return 0;
}

int
net_listen(struct Channel *file, int backlog)
{
  struct SocketEndpoint *endpoint = net_get_channel_endpoint(file);

  if (lwip_listen(endpoint->socket, backlog) != 0)
    return -errno;

  return 0;
}

int
net_connect(struct Channel *file, const struct sockaddr *address, socklen_t address_len)
{
  struct SocketEndpoint *endpoint = net_get_channel_endpoint(file);

  if (lwip_connect(endpoint->socket, address, address_len) != 0)
    return -errno;

  return 0;
}

int
net_accept(struct Channel *file, struct sockaddr *address, socklen_t * address_len,
           struct Channel **fstore)
{
  int r, conn;
  struct Channel *f;

  struct SocketEndpoint *endpoint = net_get_channel_endpoint(file);
  struct SocketEndpoint *e;

  if ((conn = lwip_accept(endpoint->socket, address, address_len)) < 0)
    return -errno;

  if ((e = k_malloc(sizeof *e)) == NULL) {
    lwip_close(conn);
    return r;
  }
  
  if ((r = channel_alloc(&f)) != 0) {
    k_free(e);
    lwip_close(conn);
    return r;
  }

  net_set_channel_endpoint(e, f, conn);

  if (fstore != NULL)
    *fstore = f;
  
  return 0;
}

int
net_close(struct Channel *file)
{
  struct SocketEndpoint *endpoint = net_get_channel_endpoint(file);

  if (lwip_close(endpoint->socket) != 0)
    return -errno;

  k_spinlock_acquire(&socket_hash.lock);
  k_list_remove(&endpoint->hash_link);
  k_spinlock_release(&socket_hash.lock);

  k_free(endpoint);

  return 0;
}

ssize_t
net_recvfrom(struct Channel *file, uintptr_t va, size_t nbytes, int flags,
             struct sockaddr *address, socklen_t *address_len)
{
  ssize_t total, r;
  char *p;

  struct SocketEndpoint *endpoint = net_get_channel_endpoint(file);

  // TODO: looks like a triple copy!
  if ((p = k_malloc(PAGE_SIZE)) == NULL)
    return -ENOMEM;

  total = 0;
  while (nbytes) {
    ssize_t nread = MIN(nbytes, PAGE_SIZE);

    r = lwip_recvfrom(endpoint->socket, p, nread, flags, address, address_len);

    if (r == 0)
      break;
      
    total += r;

    if (r < 0) {
      total = -errno;
      break;
    }

    if ((r = vm_space_copy_out(process_current(), p, va, nread)) < 0) {
      total = -errno;
      break;
    }
    
    if (r < nread)
      break;

    va     += nread;
    nbytes -= nread;
  }

  k_free(p);

  return total;
}

ssize_t
net_read(struct Channel *file, uintptr_t buf, size_t nbytes)
{
  return net_recvfrom(file, buf, nbytes, 0, NULL, NULL);
}

ssize_t
net_sendto(struct Channel *file, uintptr_t va, size_t nbytes, int flags,
           const struct sockaddr *dest_addr, socklen_t dest_len)
{
  ssize_t total, r;
  char *p;

  struct SocketEndpoint *endpoint = net_get_channel_endpoint(file);

  // TODO: looks like a triple copy!
  if ((p = k_malloc(PAGE_SIZE)) == NULL)
    return -ENOMEM;

  total = 0;
  while (nbytes) {
    ssize_t nwrite = MIN(nbytes, PAGE_SIZE);
    
    if ((r = vm_space_copy_in(process_current(), p, va, nwrite)) < 0) {
      total = -errno;
      break;
    }

    r = lwip_sendto(endpoint->socket, p, nwrite, flags, (struct sockaddr *) dest_addr, dest_len);

    if (r == 0)
      break;

    if (r < 0) {
      total = -errno;
      break;
    }

    total += r;

    if (r < nwrite)
      break;

    va     += nwrite;
    nbytes -= nwrite;
  }

  k_free(p);

  return total;
}

ssize_t
net_write(struct Channel *file, uintptr_t va, size_t nbytes)
{
  return net_sendto(file, va, nbytes, 0, NULL, 0);
}

int
net_setsockopt(struct Channel *file, int level, int option_name, const void *option_value,
               socklen_t option_len)
{
  ssize_t r;

  struct SocketEndpoint *endpoint = net_get_channel_endpoint(file);

  if ((r = lwip_setsockopt(endpoint->socket, level, option_name, option_value,
                           option_len)) < 0)
    return -errno;

  return r;
}

int
net_select(struct Channel *file, struct timeval *timeout)
{
  int r;
  fd_set dset;

  struct SocketEndpoint *endpoint = net_get_channel_endpoint(file);

  dset.__fds_bits[0] = (1 << endpoint->socket);

  if ((r = lwip_select(endpoint->socket + 1, &dset, NULL, NULL, timeout)) < 0)
    return -errno;
  
  return r;
}

int
net_gethostbyname(const char *name, ip_addr_t *addr)
{
  if (netconn_gethostbyname(name, addr) < 0)
    return -errno;
  return 0;
}
