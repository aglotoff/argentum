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
  { "/dev/tty", S_IFCHR | 0666, 0x0101 },
};

#define NDEV  (sizeof(dev_files) / sizeof(dev_files[0]))

int
main(void)
{
  struct DevFile *df;
  int status;

  char *const argv[] = { "/bin/sh", "-l", NULL };
  char *const envp[] = {
    "HOME=/home/root",
    "PATH=/usr/local/bin:/usr/bin:/bin",
    NULL,
  };

  // Create the directory for special device files.
  mkdir("/dev", 0755);
  mkdir("/etc", 0755);

  // Create missing device files.
  for (df = dev_files; df < &dev_files[NDEV]; df++)
    if ((stat("/dev/tty", NULL) < 0) && (errno == ENOENT))
      mknod("/dev/tty", S_IFCHR | 0666, 0x0101);

  mknod("/dev/zero", S_IFCHR | 0666, 0x0202);

  // Open the standard I/O streams so all other programs inherit them.
  open("/dev/tty", O_RDONLY);     // Standard input
  open("/dev/tty", O_WRONLY);     // Standard output
  open("/dev/tty", O_WRONLY);     // Standard error

  // Spawn the shell
  if (fork() == 0) {
    if (chdir("/home/root") != 0) {
      perror("chdir");
      exit(EXIT_FAILURE);
    }

    setenv("HOME", "/home/root", 1);

    setenv("PATH", "/bin:/usr/bin", 1);
    
    open("/etc/profile", O_WRONLY | O_CREAT, 0777);
    write(3, "export PS1=\"\033[1;32m[\033[0m$PWD\033[1;32m]$ \033[0m\"", 44);
    close(3);

    

    for (;;) {
      if (fork() == 0) {
        execve("/usr/bin/dash", argv, envp);
        execve("/bin/sh", argv, envp);
      } else {
        wait(NULL);
      }
    }
  }


  for (;;)
    wait(&status);

  return 0;
}
