#include <kernel/core/assert.h>
#include <lwip/sio.h>

sio_fd_t
sio_open(u8_t devnum)
{
  (void) devnum;

  k_panic("sio_open");
  return 0;
}

void
sio_send(u8_t c, sio_fd_t fd)
{
  (void) c;
  (void) fd;

  k_panic("sio_send");
}

u8_t
sio_recv(sio_fd_t fd)
{
  (void) fd;

  k_panic("sio_recv");
  return 0;
}

u32_t
sio_read(sio_fd_t fd, u8_t *data, u32_t len)
{
  (void) fd;
  (void) data;
  (void) len;

  k_panic("sio_read");
  return 0;
}

u32_t
sio_tryread(sio_fd_t fd, u8_t *data, u32_t len)
{
  (void) fd;
  (void) data;
  (void) len;

  k_panic("sio_tryread");
  return 0;
}
