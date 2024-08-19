#include <netdb.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/socket.h>

static struct hostent hostent;
static char **aliases = { NULL };
static char addr[4];
static char *addr_list[2] = { &addr[0], NULL };

struct hostent *
gethostbyname(const char *name)
{
  if (__syscall2(__SYS_GETHOSTBYNAME, name, addr) < 0)
    return NULL;

  hostent.h_name = name;
  hostent.h_aliases = aliases;
  hostent.h_addrtype = AF_INET;
  hostent.h_length = 4;
  hostent.h_addr_list = addr_list;

  return &hostent;
}
