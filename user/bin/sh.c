#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

struct Cmd;

static struct Cmd *cmd_parse(char *);
static void        cmd_run(const struct Cmd *);
static void        cmd_free(struct Cmd *);

#define MAXBUF  1024
static char buf[MAXBUF];
static const char prompt[] = "\x1b[1;32m$ \x1b[m";

static char *
get_cmd(void)
{
  size_t nread;

  write(1, prompt, sizeof(prompt)-1);

  if ((nread = read(0, buf, sizeof(buf))) < 1)
    exit(0);

  buf[nread - 1] = '\0';

  return buf;
}

int
main(void)
{
  struct Cmd *cmd;

  for (;;) {
    if ((cmd = cmd_parse(get_cmd())) == NULL)
      continue;
    cmd_run(cmd);
    cmd_free(cmd);
  }

  return 0;
}

#define MAXARG  32

#define CMD_EXEC  1
#define CMD_BG    2
#define CMD_LIST  3

struct Cmd {
  int   type;
};

struct ExecCmd {
  int   type;
  char *argv[MAXARG + 1];
  char *eargv[MAXARG];
  int   argc;
};

struct BgCmd {
  int    type;
  struct Cmd *cmd;
};

struct ListCmd {
  int   type;
  struct Cmd *left;
  struct Cmd *right;
};

static void
cmd_run(const struct Cmd * cmd)
{
  pid_t pid;
  int status;
  struct ExecCmd *ecmd;
  struct ListCmd *lcmd;
  struct BgCmd *bcmd;

  switch (cmd->type) {
  case CMD_EXEC:
    ecmd = (struct ExecCmd *) cmd;

    // CD is a special case
    if (strcmp(ecmd->argv[0], "cd") == 0) {
      if (ecmd->argv[1] == NULL) {
        printf("Usage: %s <directory>\n", ecmd->argv[0]);
        exit(1);
      }

      if (chdir(ecmd->argv[1]) != 0) {
        perror(ecmd->argv[1]);
        exit(1);
      }

      return;
    }

    if ((pid = fork()) == 0) {
      execvp(ecmd->argv[0], ecmd->argv);
      perror(ecmd->argv[0]);
      exit(1);
    } else if (pid > 0) {
      if ((pid = waitpid(pid, &status, 0)) < 0)
        perror("waitpid");
    } else {
      perror("fork");
    }
    break;

  case CMD_BG:
    bcmd = (struct BgCmd *) cmd;
    if ((pid = fork()) == 0) {
      cmd_run(bcmd->cmd);
      exit(0);
    } else if (pid < 0) {
      perror("fork");
    }
    break;

  case CMD_LIST:
    lcmd = (struct ListCmd *) cmd;
    if (lcmd->left != NULL)
      cmd_run(lcmd->left);
    if (lcmd->right != NULL)
      cmd_run(lcmd->right);
    break;
  }
}

static struct Cmd *
cmd_null_terminate(struct Cmd *cmd)
{
  int i;
  struct ExecCmd *ecmd;
  struct ListCmd *lcmd;
  struct BgCmd *bcmd;

  switch (cmd->type) {
  case CMD_EXEC:
    ecmd = (struct ExecCmd *) cmd;
    for (i = 0; i < ecmd->argc; i++)
      *ecmd->eargv[i] = '\0';
    break;

  case CMD_BG:
    bcmd = (struct BgCmd *) cmd;
    bcmd->cmd = cmd_null_terminate(bcmd->cmd);
    break;

  case CMD_LIST:
    lcmd = (struct ListCmd *) cmd;
    if (lcmd->left != NULL)
      lcmd->left = cmd_null_terminate(lcmd->left);
    if (lcmd->right != NULL)
    lcmd->right = cmd_null_terminate(lcmd->right);
    break;
  }

  return cmd;
}

static int
peek(char *s, char *tokens, char **p)
{
  if (*s == '\0')
    return 0;
  
  while (isspace(*s))
    s++;

  if (p != NULL)
    *p = s;
  
  return strchr(tokens, *s) != NULL;
}

static const char *symbols = "&;";

static int
get_token(char *s, char **p, char **ep)
{
  int ret;

  while (isspace(*s))
    s++;

  if (p != NULL)
    *p = s;
  
  switch (*s) {
  case '\0':
    return '\0';
  case '&':
  case ';':
    ret = *s++;
    break;
  default:
    ret = *s;
    while (*s && !isspace(*s) && (strchr(symbols, *s) == NULL))
      s++;
    break;
  }

  if (ep != NULL)
    *ep = s;

  return ret;
}

static struct Cmd *
cmd_parse_exec(char *s, char **ep)
{
  struct ExecCmd *cmd;
  int argc;

  if ((cmd = malloc(sizeof(struct Cmd))) == NULL) {
    perror("malloc");
    return NULL;
  }

  cmd->type = CMD_EXEC;

  for (argc = 0; !peek(s, "&;", &s); argc++) {
    if (get_token(s, &cmd->argv[argc], &s) == '\0')
      break;

    if (argc > MAXARG) {
      printf("Too many arguments (%d max)\n", MAXARG);
      free(cmd);
      return NULL;
    }

    cmd->eargv[argc] = s;
  }

  if (argc == 0) {
    free(cmd);
    return NULL;
  }

  cmd->argv[argc] = NULL;
  cmd->argc = argc;

  if (ep != NULL)
    *ep = s;

  return (struct Cmd *) cmd;
}

static struct Cmd *
cmd_parse_bg(char *s, char **p)
{
  struct Cmd *cmd;

  cmd = cmd_parse_exec(s, &s);

  while (cmd != NULL && peek(s, "&", &s)) {
    struct BgCmd *bcmd;

    get_token(s, NULL, &s);

    bcmd = (struct BgCmd *) malloc(sizeof(struct BgCmd));
    bcmd->type = CMD_BG;
    bcmd->cmd  = cmd;

    cmd = (struct Cmd *) bcmd;
  }

  if (p != NULL)
    *p = s;

  return cmd;
}

static struct Cmd *
cmd_parse_list(char *s)
{
  struct Cmd *cmd;

  cmd = cmd_parse_bg(s, &s);

  if (peek(s, ";", &s)) {
    struct ListCmd *lcmd;

    get_token(s, NULL, &s);

    if (cmd == NULL)
      return cmd_parse_list(s);

    lcmd = (struct ListCmd *) malloc(sizeof(struct ListCmd));
    lcmd->type  = CMD_LIST;
    lcmd->left  = cmd;
    lcmd->right = cmd_parse_list(s);

    return (struct Cmd *) lcmd;
  }

  return cmd;
}

static struct Cmd *
cmd_parse(char *s)
{
  struct Cmd *cmd;

  if ((cmd = cmd_parse_list(s)) == NULL)
    return NULL;

  return cmd_null_terminate(cmd);
}

static void
cmd_free(struct Cmd *cmd)
{
  struct BgCmd *bcmd;
  struct ListCmd *lcmd;

  switch (cmd->type) {
  case CMD_BG:
    bcmd = (struct BgCmd *) cmd;
    cmd_free(bcmd->cmd);
    break;
  case CMD_LIST:
    lcmd = (struct ListCmd *) cmd;
    if (lcmd->left != NULL)
      cmd_free(lcmd->left);
    if (lcmd->right != NULL)
      cmd_free(lcmd->right);
    break;
  }

  free(cmd);
}
