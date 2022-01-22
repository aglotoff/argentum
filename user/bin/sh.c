#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAXBUF  1024

static char buf[MAXBUF];
static const char prompt[] = "\x1b[1;32m$ \x1b[m";

static char **
get_cmd(void)
{
  static char *argv[33];

  size_t nread;
  char *s;
  int argc;

  write(1, prompt, sizeof(prompt)-1);

  if ((nread = read(0, buf, sizeof(buf))) < 1)
    return NULL;

  buf[nread - 1] = '\0';

  argc = 0;
  for (s = strtok(buf, " \t\n\r"); s != NULL; s = strtok(NULL, " \t\n\r")) {
    if (argc >= 32) {
      printf("Too many arguments\n");
      return NULL;
    }

    argv[argc++] = s;
  }

  if (argc == 0)
    return NULL;

  argv[argc] = NULL;
  return argv; 
}

int
main(void)
{
  char **argv;
  pid_t pid;
  int status;

  for (;;) {
    if ((argv = get_cmd()) == NULL)
      continue;
    
    if (strcmp(argv[0], "exit") == 0)
      break;

    if (strcmp(argv[0], "cd") == 0) {
      if (chdir(argv[1]) < 0)
        perror("cd");
      continue;
    }

    if ((pid = fork()) == 0) {
      execvp(argv[0], argv);
      perror(argv[0]);
      exit(1);
    } else if (pid > 0) {
      if ((pid = waitpid(pid, &status, 0)) < 0)
        perror("waitpid");
    } else {
      perror("fork");
    }
  }

  return 0;
}
