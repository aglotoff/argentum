#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mount.h>
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
};

#define NDEV  (sizeof(dev_files) / sizeof(dev_files[0]))

int
main(void)
{
  // struct DevFile *df;
  int status;
  // struct stat st;
  int i;

  // char *const argv[] = { "/bin/sh", "./run", NULL };
  char *const argv[] = { "/bin/sh", "--login", NULL };

  // Create the directory for special device files.
  mkdir("/etc", 0755);

  // Mount devfs
  mkdir("/dev", 0755);
  mount("devfs", "/dev");

  open("/etc/passwd", O_WRONLY | O_CREAT | O_TRUNC, 0777);
  write(0, "root:x:0:0:root:/root:/bin/sh\n", 30);
  close(0);

  open("/home/root/run", O_WRONLY | O_CREAT | O_TRUNC, 0777);
  write(0, "while :; do ls /usr -al | wc | wc ; date ; done\n", 48);
  close(0);

  // Spawn the shells
  for (i = 0; i < 1; i++) {
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
      setenv("PATH", "/usr/bin:/bin", 1);
      setenv("TERM", "ansi", 1);

      for (;;) {
        if (fork() == 0) {
          execv("/usr/bin/bash", argv);
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
