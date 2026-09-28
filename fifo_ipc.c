#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define FIFO_PATH "/tmp/demo_fifo"
#define BUFFER_SIZE 128

int main(void) {
    unlink(FIFO_PATH);
    if (mkfifo(FIFO_PATH, 0664) == -1) {
        perror("mkfifo failed");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        printf("[Consumer] Opening FIFO for reading (blocks until writer connects)...\n");
        int fd = open(FIFO_PATH, O_RDONLY);
        if (fd < 0) {
            perror("consumer open failed");
            exit(EXIT_FAILURE);
        }

        char rx_buf[BUFFER_SIZE];
        ssize_t bytes_read = read(fd, rx_buf, sizeof(rx_buf) - 1);
        if (bytes_read > 0) {
            rx_buf[bytes_read] = '\0';
            printf("[Consumer] Received: \"%s\" (%zd bytes)\n", rx_buf, bytes_read);
        }

        close(fd);
        exit(EXIT_SUCCESS);
    } else {
        sleep(1);
        printf("[Producer] Opening FIFO for writing...\n");
        int fd = open(FIFO_PATH, O_WRONLY);
        if (fd < 0) {
            perror("producer open failed");
            exit(EXIT_FAILURE);
        }

        const char *msg = "Payload transmitted via Kernel FIFO Buffer!";
        write(fd, msg, strlen(msg));
        printf("[Producer] Message dispatched to kernel buffer.\n");

        close(fd);
        wait(NULL);
        unlink(FIFO_PATH);
    }

    return 0;
}
