#include <kernel/drivers/eth.h>
#include <kernel/cprintf.h>
#include <kernel/fs/file.h>
#include <kernel/page.h>
#include <kernel/net.h>
#include <kernel/thread.h>

#include <lwip/api.h>
#include <lwip/dhcp.h>
#include <lwip/etharp.h>
#include <lwip/sockets.h>
#include <lwip/tcpip.h>
#include <lwip/icmp.h>
#include <lwip/inet_chksum.h>

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

  struct Page *page = page_alloc_one(0);

  pbuf_copy_partial(p, page2kva(page), p->tot_len, 0);
  eth_write(page2kva(page), p->tot_len);

  page_free_one(page);

  return ERR_OK;
}

static err_t 
eth_netif_init(struct netif *netif)
{
  extern uint8_t mac_addr[6];

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

  //sys_thread_new("ping_thread", ping_thread, NULL, DEFAULT_THREAD_STACKSIZE, DEFAULT_THREAD_PRIO);
}

void
net_init(void)
{
  tcpip_init(net_init_done, NULL);
}

int
net_socket(int domain, int type, int protocol, struct File **fstore)
{
  struct File *f;
  int r, socket;

  if ((socket = lwip_socket(domain, type, protocol)) < 0)
    return -errno;

  if ((r = file_alloc(&f)) < 0) {
    lwip_close(socket);
    return r;
  }

  f->type   = FD_SOCKET;
  f->socket = socket;
  f->ref_count++;

  if (fstore != NULL)
    *fstore = f;
  
  return 0;
}

int
net_bind(int socket, const struct sockaddr *address, socklen_t address_len)
{
  if (lwip_bind(socket, address, address_len) != 0)
    return -errno;
  return 0;
}

int
net_listen(int socket, int backlog)
{
  if (lwip_listen(socket, backlog) != 0)
    return -errno;
  return 0;
}

int
net_connect(int socket, const struct sockaddr *address, socklen_t address_len)
{
  if (lwip_connect(socket, address, address_len) != 0)
    return -errno;
  return 0;
}

int
net_accept(int socket, struct sockaddr *address, socklen_t * address_len,
           struct File **fstore)
{
  int r, conn;
  struct File *f;

  if ((conn = lwip_accept(socket, address, address_len)) < 0)
    return -errno;
  
  if ((r = file_alloc(&f)) != 0) {
    lwip_close(conn);
    return r;
  }

  f->type   = FD_SOCKET;
  f->socket = conn;
  f->flags  = O_RDWR;
  f->ref_count++;
  
  if (fstore != NULL)
    *fstore = f;
  
  return 0;
}

int
net_close(int socket)
{
  if (lwip_close(socket) != 0)
    return -errno;
  return 0;
}

ssize_t
net_recvfrom(int socket, void *buf, size_t nbytes, int flags,
             struct sockaddr *address, socklen_t *address_len)
{
  ssize_t r;

  if ((r = lwip_recvfrom(socket, buf, nbytes, flags, address, address_len)) < 0)
    return -errno;
  return r;
}

ssize_t
net_sendto(int socket, const void *buf, size_t nbytes, int flags,
           const struct sockaddr *dest_addr, socklen_t dest_len)
{
  ssize_t r;

  if ((r = lwip_sendto(socket, buf, nbytes, flags, dest_addr, dest_len)) < 0)
    return -errno;
  return r;
}

int
net_setsockopt(int socket, int level, int option_name, const void *option_value,
               socklen_t option_len)
{
  ssize_t r;

  if ((r = lwip_setsockopt(socket, level, option_name, option_value,
                           option_len)) < 0)
    return -errno;
  return r;
}
