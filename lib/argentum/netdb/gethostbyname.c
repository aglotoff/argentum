#include <netdb.h>
#include <stdio.h>
#include <sys/syscall.h>

static struct hostent hostent;

struct hostent *
gethostbyname(const char *name)
{
  if (__syscall2(__SYS_GETHOSTBYNAME, name, &hostent) < 0)
    return NULL;
  return &hostent;
}
