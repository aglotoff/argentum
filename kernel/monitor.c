#include <string.h>

#include <armv7/regs.h>
#include <drivers/console.h>
#include <cprintf.h>
#include <kdebug.h>
#include <mm/kmem.h>
#include <mm/memlayout.h>
#include <trap.h>
#include <types.h>

#include <monitor.h>

static char *read_cmd(void);
static int exec_cmd(char *s, struct TrapFrame *tf);

void
monitor(struct TrapFrame *tf)
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

    if (c == '\b') {
      if (i > 0) {
        i--;
        console_putc(c);
      }
    } else {
      console_putc(c);

      if (c == '\r' || c == '\n')
        break;

      buf[i++] = c;
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
  int        (*func)(int, char **, struct TrapFrame *);
};

static struct Command commands[] = {
  { "help", "Print this list of commands", mon_help },
  { "kerninfo", "Print this list of commands", mon_kerninfo },
  { "backtrace", "Display a list of function call frames", mon_backtrace },
  { "kmeminfo", "Display the list of object caches", mon_kmeminfo },
};

#define MAXARGS 16

/*
 * Parse and execute the command.
 */
static int
exec_cmd(char *s, struct TrapFrame *tf)
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
mon_help(int argc, char **argv, struct TrapFrame *tf)
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
mon_kerninfo(int argc, char **argv, struct TrapFrame *tf)
{
  // The following symbols are defined in the kernel.ld linker script.
  extern uint8_t _start[], _etext[], _edata[], _end[];

  (void) argc;
  (void) argv;
  (void) tf;

  cprintf("Special kernel symbols:\n");
  cprintf("  start  %p (virt)  %p (phys)\n", _start, _start);
  cprintf("  etext  %p (virt)  %p (phys)\n", _etext, PA2KVA(_etext));
  cprintf("  edata  %p (virt)  %p (phys)\n", _edata, PA2KVA(_edata));
  cprintf("  end    %p (virt)  %p (phys)\n", _end,   PA2KVA(_end));

  cprintf("Kernel executable memory footprint: %dKB\n",
          (PA2KVA(_end) - (uintptr_t) _start + 1023) / 1024);

  return 0;
}

int
mon_backtrace(int argc, char **argv, struct TrapFrame *tf)
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

  fp = (uint32_t *) (tf ? tf->r11 : r11_get());
  for ( ; fp != NULL; fp = (uint32_t *) fp[-3]) {
    debug_info_pc(fp[-1], &info);

    cprintf("  [%p] %s (%s at line %d)\n",
            fp[-1], info.fn_name, info.file, info.line);
  }

  return 0;
}

int
mon_kmeminfo(int argc, char **argv, struct TrapFrame *tf)
{
  (void) argc;
  (void) argv;
  (void) tf;

  return 0;
}
