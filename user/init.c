#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

int
main(void)
{
  int status;

  char *const argv[] = {
    "This is",
    "test",
    NULL
  };

  mkdir("/dev", 0557);

  // Create console device file
  if ((stat("/dev/console", NULL) < 0) && (errno == ENOENT)) {
    mknod("/dev/console", S_IFCHR | 0666, 0x0101);
  }

  open("/dev/console", O_RDONLY);     // Standard input
  open("/dev/console", O_WRONLY);     // Standard output
  open("/dev/console", O_WRONLY);     // Standard error

  printf("Welcome to \x1b[1;36mOSDev-PBX-A9\x1b[m!\n");

  if (fork() == 0)
    execv("/bin/sh", argv);

  while (wait(&status) > 0)
    ;

  return 0;
}
