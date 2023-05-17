#include <string.h>
#include <stdio.h>
#include <stdlib.h>

char *
strdup(const char *s1)
{
  char *s2;

  if ((s2 = malloc(strlen(s1))) == NULL)
    return NULL;

  return strcpy(s2, s1);
}
