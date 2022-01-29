#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

struct DevFile {
  const char *name;
  mode_t      mode;
  dev_t       dev;
} dev_files[] = {
  // TODO: add other devices like /dev/null or /dev/zero
  { "/dev/console", S_IFCHR | 0666, 0x0101 },
};

#define NDEV  (sizeof(dev_files) / sizeof(dev_files[0]))

int
main(void)
{
  struct DevFile *df;
  int status;

  char *const argv[] = { "/bin/sh", NULL };
  char *const envp[] = { "PATH=/bin", NULL };

  // Create the directory for special device files.
  mkdir("/dev", 0557);

  // Create missing device files.
  for (df = dev_files; df < &dev_files[NDEV]; df++)
    if ((stat("/dev/console", NULL) < 0) && (errno == ENOENT))
      mknod("/dev/console", S_IFCHR | 0666, 0x0101);

  // Open the standard I/O streams so all other programs inherit them.
  open("/dev/console", O_RDONLY);     // Standard input
  open("/dev/console", O_WRONLY);     // Standard output
  open("/dev/console", O_WRONLY);     // Standard error

  printf("Welcome to \x1b[1;36mOSDev-PBX-A9\x1b[m!\n");

  // Spawn the shell
  if (fork() == 0)
    execve("/bin/sh", argv, envp);

  // Wait for all orphaned children.
  while (wait(&status) > 0)
    ;

  return 0;
}
