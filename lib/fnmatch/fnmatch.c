#include <fnmatch.h>
#include <stddef.h>
#include <stdio.h>

int
fnmatch(const char *pattern, const char *string, int flags)
{
  const char *p = pattern, *s = string;
  const char *psave = NULL, *ssave = NULL;

  (void) flags;

  while (*s != '\0') {
    if ((*p == *s) || (*p == '?')) {
      p++;
      s++;
    } else if (*p == '*') {
      while (*p == '*')
        p++;

      if (*p == '\0')
        return 0;

      psave = p;
      ssave = s;
    } else if (psave != NULL) {
      p = psave;
      s = ++ssave;
    } else {
      return FNM_NOMATCH;
    }
  }

  while (*p == '*')
    p++;

  return (*p == '\0') ? 0 : FNM_NOMATCH;
}
