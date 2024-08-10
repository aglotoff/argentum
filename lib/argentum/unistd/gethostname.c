#include <unistd.h>
#include <string.h>

int
gethostname (char *name, size_t len)
{
  strncpy(name, "localhost", len);
  return 0;
}
