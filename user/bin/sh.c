#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

struct Cmd;

static struct Cmd *cmd_parse(char *);
static void        cmd_run(const struct Cmd *);
static void        cmd_free(struct Cmd *);

#define MAXBUF  1024
static char buf[MAXBUF];
static char cwd[PATH_MAX];

#define MAXARG      32

#define CMD_EXEC    1
#define CMD_REDIR   2
#define CMD_BG      3
#define CMD_LIST    4

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

struct RedirCmd {
  int    type;
  struct Cmd *cmd;
  int    fd;
  char  *name;
  char  *ename;
  int    oflag;
};

struct ListCmd {
  int   type;
  struct Cmd *left;
  struct Cmd *right;
};

static char *
get_cmd(void)
{
  static const char *home = "/home/root";

  size_t nread, n;

  printf("\x1b[1;32m[ \x1b[m");

  n = strlen(home);
  if ((strncmp(cwd, home, n) == 0) && ((cwd[n] == '/') || (cwd[n] == '\0'))) {
    printf("~%s", &cwd[n]);
  } else {
    printf(cwd);
  }

  printf("\x1b[1;32m ]$ \x1b[m");

  if ((nread = read(0, buf, sizeof(buf))) < 1)
    exit(0);

  buf[nread - 1] = '\0';

  return buf;
}

static int
builtin_cd(int argc, char **argv)
{
  if (argc < 2) {
    printf("Usage: %s <directory>\n", argv[0]);
    return EXIT_FAILURE;
  }

  if (chdir(argv[1]) != 0) {
    perror(argv[1]);
    return EXIT_FAILURE;
  } else if (getcwd(cwd, sizeof(cwd)) == NULL) {
    perror("getcwd");
    return EXIT_FAILURE;
  }
  
  return 0;
}

static int
builtin_export(int argc, char **argv)
{
  char *p;
  
  if (argc < 2) {
    printf("Usage: %s name=word\n", argv[0]);
    return EXIT_FAILURE;
  }

  if ((p = strchr(argv[1], '=')) == NULL) {
    printf("Usage: %s name=word\n", argv[0]);
    return EXIT_FAILURE;
  }

  *p++ = '\0';
  if (setenv(argv[1], p, 1) != 0) {
    perror("setenv");
    return EXIT_FAILURE;
  }
  
  return 0;
}

struct BuiltinCmd {
  const char  *name;
  int        (*func)(int, char **);
};

static struct BuiltinCmd builtins[] = {
  { "cd",     builtin_cd     },
  { "export", builtin_export },
};

#define NBUILTINS   (sizeof(builtins) / sizeof(builtins[0]))

int
main(void)
{
  struct Cmd *cmd; 

  umask(S_IWGRP | S_IWOTH);

  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    perror("getcwd");
    exit(EXIT_FAILURE);
  }
  
  for (;;) {
    if ((cmd = cmd_parse(get_cmd())) == NULL)
      continue;

    cmd_run(cmd);
    cmd_free(cmd);
  }

  return 0;
}

static void
cmd_run(const struct Cmd * cmd)
{
  pid_t pid;
  int fd;
  int status;
  struct ExecCmd *ecmd;
  struct ListCmd *lcmd;
  struct BgCmd *bcmd;
  struct RedirCmd *rcmd;
  struct BuiltinCmd *builtin;

  switch (cmd->type) {
  case CMD_EXEC:
    ecmd = (struct ExecCmd *) cmd;

    for (builtin = builtins; builtin < &builtins[NBUILTINS]; builtin++)
      if (strcmp(ecmd->argv[0], builtin->name) == 0) {
        builtin->func(ecmd->argc, ecmd->argv);
        return;
      }

    if ((pid = fork()) == 0) {
      execvp(ecmd->argv[0], ecmd->argv);
      perror(ecmd->argv[0]);
      exit(1);
    } else if (pid > 0) {
      waitpid(pid, &status, 0);
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

    if (lcmd->left != NULL) {
      cmd_run(lcmd->left);
    }

    if (lcmd->right != NULL) {
      cmd_run(lcmd->right);
    }

    break;

  case CMD_REDIR:
    rcmd = (struct RedirCmd *) cmd;

    close(rcmd->fd);

    if ((fd = open(rcmd->name, rcmd->oflag, 0666)) < 0) {
      perror(rcmd->name);
      exit(1);
    }

    cmd_run(rcmd->cmd);
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
  struct RedirCmd *rcmd;

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

  case CMD_REDIR:
    rcmd = (struct RedirCmd *) cmd;
    *rcmd->ename = '\0';
    rcmd->cmd = cmd_null_terminate(rcmd->cmd);
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

static const char *symbols = "&;<>";

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
  case '<':
    ret = *s++;
    break;
  case '>':
    if (*++s == '>') {
      ret = '+';
      s++;
    } else {
      ret = '>';
    }
    break;
  default:
    ret = 'a';
    while (*s && !isspace(*s) && (strchr(symbols, *s) == NULL))
      s++;
    break;
  }

  if (ep != NULL)
    *ep = s;

  return ret;
}

static struct Cmd *
cmd_parse_redir(struct Cmd *cmd, char *s, char **ep)
{
  struct RedirCmd *rcmd;
  
  while (peek(s, "<>", &s)) {
    rcmd = (struct RedirCmd *) malloc(sizeof(struct RedirCmd));

    rcmd->type = CMD_REDIR;
    rcmd->cmd  = cmd;

    switch (get_token(s, NULL, &s)) {
    case '<':
      rcmd->fd = 0;
      rcmd->oflag = O_RDONLY;
      break;
    case '>':
      rcmd->fd = 1;
      rcmd->oflag = O_WRONLY | O_CREAT | O_TRUNC;
      break;
    case '+':
      rcmd->fd = 1;
      rcmd->oflag = O_WRONLY | O_CREAT | O_APPEND;
      break;
    }

    get_token(s, &rcmd->name, &s);
    rcmd->ename = s;

    cmd = (struct Cmd *) rcmd;
  }

  if (ep != NULL)
    *ep = s;
  
  return cmd;
}

static struct Cmd *
cmd_parse_exec(char *s, char **ep)
{
  struct ExecCmd *cmd;
  struct Cmd *ret;
  int argc;

  if ((cmd = malloc(sizeof(struct ExecCmd))) == NULL) {
    perror("malloc");
    return NULL;
  }

  cmd->type = CMD_EXEC;

  ret = (struct Cmd *) cmd;
  ret = cmd_parse_redir(ret, s, &s);

  for (argc = 0; !peek(s, "&;", &s); argc++) {
    if (get_token(s, &cmd->argv[argc], &s) == '\0')
      break;

    if (argc > MAXARG) {
      printf("Too many arguments (%d max)\n", MAXARG);
      free(cmd);
      return NULL;
    }

    cmd->eargv[argc] = s;

    ret = cmd_parse_redir(ret, s, &s);
  }

  if (argc == 0) {
    free(cmd);
    return NULL;
  }

  cmd->argv[argc] = NULL;
  cmd->argc = argc;

  if (ep != NULL)
    *ep = s;

  return ret;
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
  struct RedirCmd *rcmd;

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
  case CMD_REDIR:
    rcmd = (struct RedirCmd *) cmd;
    cmd_free(rcmd->cmd);
    break;
  }

  free(cmd);
}
