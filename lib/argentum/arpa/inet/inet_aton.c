#include <arpa/inet.h>
#include <ctype.h>
#include <stdio.h>

#define NPARTS  4

int
inet_aton(const char *cp, struct in_addr *inp)
{
  uint32_t parts[NPARTS];
  int i, part_count = 0;
  in_addr_t result = 0;

  for (;;) {
    uint32_t value = 0;
    int base;

    if (*cp == '0') {
      cp++;
      if (*cp == 'x' || *cp == 'X') {
        cp++;
        base = 16;
      } else {
        base = 8;
      }
    } else {
      base = 10;
    }

    if ((base == 16) && !isxdigit(*cp))
      return 0;
    if ((base == 10) && !isdigit(*cp))
      return 0;

    while (isxdigit(*cp)) {
      uint32_t new_value;
      int digit;

      if (isdigit(*cp))
        digit = *cp - '0';
      else
        digit = tolower(*cp) - 'a' + 10;

      if (digit >= base)
        return 0;
      
      new_value = value * base + digit;
      if (new_value < value)
        return 0;
      
      value = new_value;
      cp++;
    }

    parts[part_count++] = value;

    if (*cp == '\0')
      break;
    
    if ((*cp != '.') || (part_count >= NPARTS) || (value > 255))
      return 0;

    cp++;
  }

  switch (part_count) {
  case 1:
    break;
  case 2:
    if (parts[1] > 0xFFFFFF)
      return 0;
    result = (parts[0] << 24) | parts[1];
    break;
  case 3:
    if (parts[2] > 0xFFFF)
      return 0;
    result = (parts[0] << 24) | (parts[1] << 16) | parts[2];
    break;
  case 4:
    result = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
    break;
  }

  if (inp != NULL)
    inp->s_addr = htonl(result);
  
  return 1;
}
