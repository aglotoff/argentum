#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAXBUF  1024

static char buf[MAXBUF];
static const char prompt[] = "\x1b[1;32m$ \x1b[m";

static char *
get_cmd(void)
{
  size_t nread;

  write(1, prompt, sizeof(prompt)-1);

  if ((nread = read(0, buf, sizeof(buf))) < 1)
    return NULL;

  buf[nread - 1] = '\0';
  return strtok(buf, " \t\n\r");
}

int
main(void)
{
  char *cmd;
  char *argv[2];
  pid_t pid;
  int status;


  for (;;) {
    if ((cmd = get_cmd()) == NULL)
      continue;
    
    if (strcmp(cmd, "exit") == 0)
      break;
    
    if ((pid = fork()) == 0) {
      argv[0] = cmd;
      argv[1] = NULL;
  
      exec(cmd, argv);
      perror(cmd);
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
