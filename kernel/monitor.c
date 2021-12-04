#include <string.h>

#include "armv7.h"
#include "console.h"
#include "kdebug.h"
#include "kernel.h"
#include "kobject.h"
#include "memlayout.h"
#include "monitor.h"
#include "trap.h"

static char *read_cmd(void);
static int exec_cmd(char *s, struct Trapframe *tf);

void
monitor(struct Trapframe *tf)
{
  char *s;

  cprintf("Entering the kernel monitor\n"
          "Type `help' to see the list of commands.\n");

  for (;;) {
    if ((s = read_cmd()) != NULL) {
      if (exec_cmd(s, tf) < 0)
        break;
    }
  }
}

/*
 * ----------------------------------------------------------------------------
 * Input buffer
 * ----------------------------------------------------------------------------
 */

#define BUFSIZE 256

/**
 * Display prompt and read a command from the console.
 */
static char *
read_cmd(void)
{
  static char buf[BUFSIZE];

  int c, i = 0;

  cprintf("K> ");

  while (i < BUFSIZE-1) {
    c = console_getc();

    if (c == '\r' || c == '\n') {
      console_putc('\n');
      break;
    }

    if (c == '\b' && i > 0) {
      i--;
      console_putc(c);
    } else if (c != '\b') {
      buf[i++] = c;
      console_putc(c);
    }
  }

  // Null-terminate the string
  buf[i] = '\0';

  return i > 0 ? buf : NULL;
}

/*
 * ----------------------------------------------------------------------------
 * List of comands
 * ----------------------------------------------------------------------------
 */

struct Command {
  const char  *name;
  const char  *desc;
  int        (*func)(int, char **, struct Trapframe *);
};

static struct Command commands[] = {
  { "help", "Print this list of commands", mon_help },
  { "kerninfo", "Print this list of commands", mon_kerninfo },
  { "backtrace", "Display a list of function call frames", mon_backtrace },
  { "poolinfo", "Display the list of object pools", mon_poolinfo },
};

#define MAXARGS 16

/*
 * Parse and execute the command.
 */
static int
exec_cmd(char *s, struct Trapframe *tf)
{
  static const char *const whitespace = " \t\r\n\f";

  struct Command *cmd;
  char *argv[MAXARGS+1];
  int argc = 0;

  // Parse the command name and arguments.
  for (;;) {
    while (*s && strchr(whitespace, *s))
      *s++ = '\0';

    if (*s == '\0')
      break;

    if (argc >= MAXARGS) {
      cprintf("Too many arguments (max=%u)\n", MAXARGS);
      return 0;
    }

    argv[argc++] = s;

    while (*s && !strchr(whitespace, *s))
      s++;
  }

  argv[argc] = NULL;

  // Empty line
  if (argc == 0)
    return 0;

  // Search for a command with the given name
  for (cmd = commands; cmd < &commands[ARRAY_SIZE(commands)]; cmd++) {
    if (strcmp(argv[0], cmd->name) == 0)
      return cmd->func(argc, argv, tf);
  }

  cprintf("Unknown command `%s'\n", argv[0]);
  return 0;
}


/***** Implementations of kernel monitor commands. *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
  struct Command *cmd;

  (void) argc;
  (void) argv;
  (void) tf;

  for (cmd = commands; cmd < &commands[ARRAY_SIZE(commands)]; cmd++) {
    cprintf("  %-10s %s\n", cmd->name, cmd->desc);
  }

  return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
  // The following symbols are defined in the kernel.ld linker script.
  extern uint8_t _start[], _etext[], _edata[], _end[];

  (void) argc;
  (void) argv;
  (void) tf;

  cprintf("Special kernel symbols:\n");
  cprintf("  \x1b[1;33mstart  \x1b[32m%p (virt)  \x1b[36m%p (phys)\x1b[0m\n",
          _start, _start);
  cprintf("  \x1b[1;33metext  \x1b[32m%p (virt)  \x1b[36m%p (phys)\x1b[0m\n",
          _etext, PADDR(_etext));
  cprintf("  \x1b[1;33medata  \x1b[32m%p (virt)  \x1b[36m%p (phys)\x1b[0m\n",
          _edata, PADDR(_edata));
  cprintf("  \x1b[1;33mend    \x1b[32m%p (virt)  \x1b[36m%p (phys)\x1b[0m\n",
          _end,   PADDR(_end));

  cprintf("Kernel executable memory footprint: \x1B[1;35m%dKB\x1B[0m\n",
          (PADDR(_end) - (uintptr_t) _start + 1023) / 1024);

  return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
  struct PcDebugInfo info;
  uint32_t *fp;
  
  (void) argc;
  (void) argv;

  cprintf("Stack backtrace:\n");

  // Display stack backtrace by chasing the saved frame pointer values
  //
  // The structure is as follows:
  // fp points here:  | save code pointer |  fp[0]
  //                  | return link value |  fp[-1]
  //                  | return sp value   |  fp[-2]
  //                  | return fp value   |  fp[-3]
  //
  // The code needs to be compiled with the -mapcs-frame and
  // -fno-omit-frame-pointer flags

  fp = (uint32_t *) (tf ? tf->r11 : read_fp());
  for ( ; fp != NULL; fp = (uint32_t *) fp[-3]) {
    debug_info_pc(fp[-1], &info);

    cprintf("  [\x1B[1;35m%p\x1B[0m] \x1B[1;33m%s\x1B[0m "
            "(\x1B[1;36m%s\x1B[0m at line \x1B[1;36m%d\x1B[0m)\n",
            fp[-1], info.fn_name, info.file, info.line);
  }

  return 0;
}

int
mon_poolinfo(int argc, char **argv, struct Trapframe *tf)
{
  (void) argc;
  (void) argv;
  (void) tf;

  kobject_pool_info();

  return 0;
}
