#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

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

#define BUFSIZE 1024
static char proto_buf[BUFSIZE];

static FILE *proto_file;

static struct protoent protoent;
static char *proto_aliases[32];

struct protoent *
getprotoent(void)
{
  char *s;

  if ((proto_file == NULL) &&
     ((proto_file = fopen(_PATH_PROTOCOLS, "r")) == NULL))
    return NULL;
  
  while ((s = fgets(proto_buf, BUFSIZE, proto_file)) != NULL) {
    char *p;
    int j;

    // Strip the comment or the terminating newline
    if ((p = strpbrk(s, "#\n")) != NULL)
      *p = '\0';

    protoent.p_name = next_item(s, &s);

    // The name is required
    if (protoent.p_name == NULL)
      continue;
  
    if ((p = next_item(s, &s)) == NULL)
      continue;

    protoent.p_proto = atoi(p);

    for (j = 0; (j < 31) && (s != NULL); j++)
      if ((proto_aliases[j] = next_item(s, &s)) == NULL)
        break;
    proto_aliases[j] = NULL;
    protoent.p_aliases = proto_aliases;
    
    return &protoent;
  }

  return NULL;
}

static char serv_buf[BUFSIZE];

static FILE *serv_file;

static struct servent servent;
static char *serv_aliases[32];

struct servent *
getservent(void)
{
  char *s;

  if ((serv_file == NULL) && ((serv_file = fopen(_PATH_SERVICES, "r")) == NULL))
    return NULL;
  
  while ((s = fgets(serv_buf, BUFSIZE, serv_file)) != NULL) {
    char *p;
    int j;

    // Strip the comment or the terminating newline
    if ((p = strpbrk(s, "#\n")) != NULL)
      *p = '\0';

    servent.s_name = next_item(s, &s);

    // The name is required
    if (servent.s_name == NULL)
      continue;
  
    if ((p = next_item(s, &s)) == NULL)
      continue;

    servent.s_port = atoi(p);
    if ((p = strchr(p, '/')) == NULL)
      continue;

    *p++ = '\0';
    servent.s_proto = p;

    for (j = 0; (j < 31) && (s != NULL); j++)
      if ((serv_aliases[j] = next_item(s, &s)) == NULL)
        break;
    serv_aliases[j] = NULL;
    servent.s_aliases = serv_aliases;

    return &servent;
  }

  return NULL;
}
