#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_PORT 8088
#define BUFFER_SIZE 64

void *server_worker(void *arg) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SERVER_PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 1);
    printf("[Server] Listening on port %d...\n", SERVER_PORT);

    int client_fd = accept(server_fd, NULL, NULL);
    printf("[Server] Connection accepted (New client FD: %d)\n", client_fd);

    char buffer[BUFFER_SIZE];
    ssize_t bytes = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("[Server] Echoing back: %s\n", buffer);
        write(client_fd, buffer, bytes);
    }

    close(client_fd);
    close(server_fd);
    return NULL;
}

int main(void) {
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, server_worker, NULL);
    sleep(1);

    printf("[Client] Creating TCP socket...\n");
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    printf("[Client] Connecting to 127.0.0.1:%d...\n", SERVER_PORT);
    if (connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect failed");
        exit(EXIT_FAILURE);
    }

    const char *payload = "Kernel Socket Data Test";
    write(sock_fd, payload, strlen(payload));

    char response[BUFFER_SIZE];
    ssize_t received = read(sock_fd, response, sizeof(response) - 1);
    response[received] = '\0';
    printf("[Client] Server response received: \"%s\"\n", response);

    close(sock_fd);
    pthread_join(thread_id, NULL);
    return 0;
}
