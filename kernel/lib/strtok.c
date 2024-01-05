#include <string.h>

/**
 * @brief Split string into tokens.
 * 
 * 
 * 
 * @sa memchr, strchr, strcspn, strpbrk, strrchr, strspn, strstr
 */
char *
strtok(char *s, const char *sep)
{
  static char *state = "";
  char *begin, *end;

  begin = (s != NULL) ? s : state;
  begin += strspn(begin, sep);
  if (*begin == '\0')
    return NULL;

  if ((end = strpbrk(begin, sep)) != NULL) {
    *end++ = '\0';
    state = end;
  } else {
    state = "";
  }

  return begin;
}
