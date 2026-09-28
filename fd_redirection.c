#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int main(void) {
    int fd = open("output.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open failed");
        exit(EXIT_FAILURE);
    }

    printf("[Console] Output file opened. Assigned File Descriptor: %d\n", fd);
    printf("[Console] Now redirecting STDOUT (FD 1) to 'output.log'...\n");

    if (dup2(fd, STDOUT_FILENO) < 0) {
        perror("dup2 failed");
        close(fd);
        exit(EXIT_FAILURE);
    }

    close(fd);

    printf("This text no longer prints to the terminal console!\n");
    printf("It was written through redirected File Descriptor 1.\n");
    fflush(stdout);

    return 0;
}
