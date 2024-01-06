
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>

int
inet_pton(int af, const char *src, void *dst)
{
  uint32_t parts[4];
  uint32_t value = 0;
  int part_count = 0;

  if (af != AF_INET) {
    // TODO: AF_INET6
    errno = EAFNOSUPPORT;
    return -1;
  }

  for (;;) {
    uint32_t current_part = 0;

    if (!isdigit(*src))
      return 0;

    while (isdigit(*src)) {
      current_part = current_part * 10 + (*src - '0');
      src++;
      
      if (current_part > 255)
        return 0;
    }

    parts[part_count++] = current_part;

    if (*src == '\0')
      break;
    if (*src != '.')
      return 0;

    src++;
  }

  if (part_count != 4)
    return 0;

  value = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];

  if (dst != NULL)
    *(uint32_t *) dst = htonl(value);
  
  return 1;
}
