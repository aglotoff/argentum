
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>

#define NPARTS  4

int
inet_pton(int af, const char *src, void *dst)
{
  uint32_t parts[NPARTS];
  int part_count = 0;
  in_addr_t result = 0;

  if (af != AF_INET) {
    // TODO: support for AF_INET6
    errno = EAFNOSUPPORT;
    return -1;
  }

  for (;;) {
    uint32_t value = 0;

    if (!isdigit(*src))
      return 0;

    while (isdigit(*src)) {
      value = value * 10 + (*src - '0');
      src++;

      if (value > 255)
        return 0;
    }

    parts[part_count++] = value;

    if (*src == '\0')
      break;

    if ((*src != '.') || (part_count >= NPARTS))
      return 0;

    src++;
  }

  if (part_count != NPARTS)
    return 0;

  result = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];

  if (dst != NULL)
    *(in_addr_t *) dst = htonl(result);
  
  return 1;
}
