#include <stdio.h>

char *
gets(char *s)
{
  int i, c;

  for (i = 0; (c = getchar()) != '\n'; i++) {
    if (c == EOF)
      return NULL;
    s[i] = c;
  }

  s[i] = '\0';

  return s;
}
