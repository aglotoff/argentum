#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define DEPTH 3

void forktree(int);

void
forkchild(int depth)
{
	if (depth >= DEPTH)
		return;

	if (fork() == 0) {
		forktree(depth + 1);
		exit(depth);
	}
}

void
forktree(int depth)
{
	int status;
	
	printf("I am process %04x, my parent is %04x\n", getpid(), getppid());

	forkchild(depth);
	forkchild(depth);

	waitpid(-1, &status, 0);
	waitpid(-1, &status, 0);
}

int
main(void)
{
	forktree(0);
  return 0;
}
