#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <inttypes.h>
#include <sys/wait.h>

int n = 10;

int main()
{
    
    pid_t pid = vfork(); //creating the child process
    if (pid == 0)          //if this is a chile process
    {
        write(1, "Child process started\n", 23);
        n = 121;
    }
    else//parent process execution
    {
      waitpid(pid, NULL, 0);
      write(1, "Now i am coming back to parent process\n", 40);
      
    }
    //fprintf(pid ? stderr : stdout, "value of n: %d \n",n); //sample printing to check "n" value
    
    //write(1, "Rett\n", 5);
    exit(n);
    
    return 0;
}