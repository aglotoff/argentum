#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
//
struct DevFile {
  const char *name;
  mode_t      mode;
  dev_t       dev;
} dev_files[] = {
  // TODO: add other devices like /dev/null or /dev/zero
  { "/dev/tty0", S_IFCHR | 0666, 0x0100 },
  { "/dev/tty1", S_IFCHR | 0666, 0x0101 },
  { "/dev/tty2", S_IFCHR | 0666, 0x0102 },
  { "/dev/tty3", S_IFCHR | 0666, 0x0103 },
  { "/dev/tty4", S_IFCHR | 0666, 0x0104 },
  { "/dev/tty5", S_IFCHR | 0666, 0x0105 },
  { "/dev/zero", S_IFCHR | 0666, 0x0202 },
};

#define NDEV  (sizeof(dev_files) / sizeof(dev_files[0]))

int
main(void)
{
  struct DevFile *df;
  int status;
  int i;

  char *const argv[] = { "/bin/sh", "-l", NULL };

  // Create the directory for special device files.
  mkdir("/dev", 0755);
  mkdir("/etc", 0755);

  // Create missing device files.
  for (df = dev_files; df < &dev_files[NDEV]; df++)
    if ((stat(df->name, NULL) < 0) && (errno == ENOENT))
      mknod(df->name, df->mode, df->dev);

  open("/etc/profile", O_WRONLY | O_CREAT, 0777);
  write(0, "export PS1=\"\033[1;32m[\033[0m$PWD\033[1;32m]$ \033[0m\"", 44);
  close(0);

  // Spawn the shells
  for (i = 0; i < 6; i++) {
    if (fork() == 0) {
      // Open the standard I/O streams so all other programs inherit them.
      open(dev_files[i].name, O_RDONLY);     // Standard input
      open(dev_files[i].name, O_WRONLY);     // Standard output
      open(dev_files[i].name, O_WRONLY);     // Standard error

      if (chdir("/home/root") != 0) {
        perror("chdir");
        exit(EXIT_FAILURE);
      }

      setenv("HOME", "/home/root", 1);
      setenv("PATH", "/bin:/usr/bin", 1);
      setenv("TERM", "ansi", 1);

      for (;;) {
        if (fork() == 0) {
          execv("/usr/bin/dash", argv);
          execv("/bin/sh", argv);
        } else {
          wait(NULL);
        }
      }
    }
  }

  for (;;)
    wait(&status);

  return 0;
}
