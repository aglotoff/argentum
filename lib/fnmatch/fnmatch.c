#include <fnmatch.h>
#include <stddef.h>
#include <stdio.h>

static int match_range(const char *, char, int, const char **);

int
fnmatch(const char *pattern, const char *string, int flags)
{
  const char *p = pattern, *s = string;
  const char *psave = NULL, *ssave = NULL;

  (void) flags;

  while (*s != '\0') {
    if (*p == '*') {
      while (*p == '*')
        p++;

      if (*p == '\0')
        return 0;

      psave = p;
      ssave = s;
    } else if ((*p == '[') && match_range(p, *s, flags, &p)) {
      s++;
    } else if ((*p == '?') || ((*p != '[') && (*p == *s))) {
      p++;
      s++;
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

static int
match_range(const char *pattern, char c, int flags, const char **end)
{
  int match = 0, not = 0;
  const char *p = pattern;

  (void) flags;
  (void) c;

  if (*++p == '!') {
    p++;
    not = 1;
  }

  if (*p == ']')
    match = (*p++ == c);

  while (*p != ']') {
    if (*p == '\0') {
      *end = ++pattern;
      return (c == '[');
    }

    if ((p[1] == '-') && (p[2] != ']') && (p[2] != '\0')) {
      match |= (c >= p[0]) && (c <= p[2]);
      p += 3;
    } else {
      match |= (*p == c);
      p++;
    }
  }

  *end = ++p;
  
  return not ? !match : match;
}
