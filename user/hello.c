#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define BUFSIZE 1024
char buf[BUFSIZE];

static char *
next_item(char *s, char **rest)
{
  char *start;

  while (isblank(*s))
    s++;

  start = *s ? s : NULL;

  while (*s && !isblank(*s))
    s++;

  // Null-terminate the current item
  if (*s)
    *s++ = '\0';

  *rest = *s ? s : NULL;

  return start;
}

int
main(void)
{
  FILE *file;
  int i;

  if ((file = fopen("/etc/protocols", "r")) == NULL) {
    perror("/etc/protocols");
    exit(EXIT_FAILURE);
  }

  for (i = 0; fgets(buf, BUFSIZE, file) != NULL; i++) {
    struct protoent entry;
    char *aliases[32];
    char *s, *p;
    int j;

    s = buf;
    
    // Strip the comment or the terminating newline
    if ((p = strpbrk(s, "#\n")) != NULL)
      *p = '\0';

    entry.p_name = next_item(s, &s);

    // The name is required
    if (entry.p_name == NULL)
      continue;
  
    if ((p = next_item(s, &s)) == NULL)
      continue;

    entry.p_proto = atoi(p);

    for (j = 0; (j < 31) && (s != NULL); j++)
      if ((aliases[j] = next_item(s, &s)) == NULL)
        break;
    aliases[j] = NULL;

    entry.p_aliases = aliases;
    
    printf("name: %s, proto: %d\n", entry.p_name, entry.p_proto);
    for (char **a = &entry.p_aliases[0]; *a != NULL; a++)
      printf("  %s\n", *a);
  }

  fclose(file);

  return 0;
}
