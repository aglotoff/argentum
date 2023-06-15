#include <kernel/drivers/eth.h>
#include <kernel/cprintf.h>
#include <kernel/mm/page.h>
#include <kernel/net.h>
#include <kernel/task.h>

#include <lwip/api.h>
#include <lwip/dhcp.h>
#include <lwip/etharp.h>
#include <lwip/tcpip.h>

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

static struct Task echo_task;
static uint8_t echo_task_stack[4096];

void
tcp_echo(void *arg)
{
  struct netconn *conn, *newconn;
  err_t err, accept_err;
  struct netbuf* buf;
  void* data;
  u16_t len;
  err_t recv_err;

  (void) arg;

  if ((conn = netconn_new(NETCONN_TCP)) != NULL) {
    if ((err = netconn_bind(conn, NULL, 80)) == ERR_OK) {
      netconn_listen(conn);

      for (;;) {
        if ((accept_err = netconn_accept(conn, &newconn)) == ERR_OK) {
          while ((recv_err = netconn_recv(newconn, &buf)) == ERR_OK) {
            do {
              netbuf_data(buf, &data, &len);
              netconn_write(newconn, data, len, NETCONN_COPY);
            } while (netbuf_next(buf) >= 0);

            netbuf_delete(buf);
          }

          netconn_close(newconn);
        } else {
          cprintf("can not bind TCP netconn");
        }

        netconn_delete(newconn);
      }
    }
  }
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

  task_create(&echo_task, tcp_echo, NULL, 0, echo_task_stack + 4096, NULL);
  task_resume(&echo_task);
}

void
net_init(void)
{
  tcpip_init(net_init_done, NULL);
}
