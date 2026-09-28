#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(void) {
    printf("[Main] Parent PID: %d, Parent PPID (Shell): %d\n", getpid(), getppid());

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        printf("[Child] Running -> PID: %d, Parent PID: %d\n", getpid(), getppid());
        printf("[Child] Simulating work for 2 seconds...\n");
        sleep(2);
        printf("[Child] Exiting with code 42.\n");
        exit(42);
    } else {
        printf("[Parent] Created child process with PID: %d\n", pid);
        
        int status;
        pid_t child_pid = waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            printf("[Parent] Reaped Child PID %d cleanly. Exit Status: %d\n", 
                   child_pid, WEXITSTATUS(status));
        }
    }

    return 0;
}
