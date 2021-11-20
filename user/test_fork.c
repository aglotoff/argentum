#include <stdio.h>
#include <stdlib.h>
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
		exit(0);
	}
}

void
forktree(int depth)
{
	printf("I am process %04x, my parent is %04x\n", getpid(), getppid());

	forkchild(depth);
	forkchild(depth);
}

int
main(void)
{
	forktree(0);
  return 0;
}
