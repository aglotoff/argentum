#include <stdio.h>

char *
fgets(char *s, int n, FILE *stream)
{
  int i;

  // TODO: does it make any sence to return an empty string if n == 1?
  if (n < 1)
    return NULL;

  for (i = 0; i < (n - 1); i++) {
    int c = getc(stream);

    if (c == EOF)
      break;

    s[i] = c;

    if (c == '\n')
      break;
  }

  // Either an error occured or an EOF condition encountered before any bytes
  // were read
  if (ferror(stream) || (feof(stream) && (i == 0)))
    return NULL;

  s[i] = '\0';

  return s;
}
