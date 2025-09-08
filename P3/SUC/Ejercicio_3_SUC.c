// Servidor TCP optimizado con manejo concurrente de conexiones
// Usa el mismo protocolo que server.c (TARGET_PORT, SHIFT, CONTENT_LENGTH)
// y aplica cifrado César en caso de que el TARGET_PORT coincida con su puerto.
// Optimiza usando procesos hijo (fork) por conexión y lectura eficiente.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BACKLOG 64
#define RECV_BUF 4096

static void caesar_encrypt_inplace(char *text, size_t len, int shift) {
    if (!text) return;
    shift %= 26;
    if (shift < 0) shift += 26;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)text[i];
        if (isupper(c)) {
            text[i] = (char)(((c - 'A' + shift) % 26) + 'A');
        } else if (islower(c)) {
            text[i] = (char)(((c - 'a' + shift) % 26) + 'a');
        }
    }
}

static int create_listen_socket(int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }
    int yes = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sockfd);
        return -1;
    }
    if (listen(sockfd, BACKLOG) < 0) {
        perror("listen");
        close(sockfd);
        return -1;
    }
    return sockfd;
}

static int recv_line(int sockfd, char *buf, size_t maxlen) {
    size_t total = 0;
    while (total + 1 < maxlen) {
        char c;
        ssize_t r = recv(sockfd, &c, 1, 0);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) break;
        buf[total++] = c;
        if (c == '\n') break;
    }
    buf[total] = '\0';
    return (int)total;
}

static void sigchld_handler(int signo) {
    (void)signo;
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
}

static void handle_client(int cfd, int server_port) {
    char line[RECV_BUF];
    int target_port = -1;
    int shift = 0;
    size_t content_length = 0;

    if (recv_line(cfd, line, sizeof(line)) <= 0) { close(cfd); _exit(0); }
    if (sscanf(line, "TARGET_PORT %d", &target_port) != 1) { close(cfd); _exit(0); }

    if (recv_line(cfd, line, sizeof(line)) <= 0) { close(cfd); _exit(0); }
    if (sscanf(line, "SHIFT %d", &shift) != 1) { close(cfd); _exit(0); }

    if (recv_line(cfd, line, sizeof(line)) <= 0) { close(cfd); _exit(0); }
    if (sscanf(line, "CONTENT_LENGTH %zu", &content_length) != 1) { close(cfd); _exit(0); }

    if (recv_line(cfd, line, sizeof(line)) <= 0) { close(cfd); _exit(0); }

    char *content = (char *)malloc(content_length);
    if (!content) { close(cfd); _exit(0); }
    size_t total = 0;
    while (total < content_length) {
        ssize_t r = recv(cfd, content + total, content_length - total, 0);
        if (r < 0) { if (errno == EINTR) continue; free(content); close(cfd); _exit(0); }
        if (r == 0) break;
        total += (size_t)r;
    }
    if (total != content_length) { free(content); close(cfd); _exit(0); }

    if (target_port == server_port) {
        caesar_encrypt_inplace(content, content_length, shift);
        const char *ok = "PROCESSED\n";
        send(cfd, ok, strlen(ok), 0);
    } else {
        const char *rej = "REJECTED\n";
        send(cfd, rej, strlen(rej), 0);
    }

    free(content);
    close(cfd);
    _exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <PUERTO_SERVIDOR>\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s 49200\n", argv[0]);
        return 1;
    }
    int server_port = atoi(argv[1]);

    signal(SIGCHLD, sigchld_handler);

    int listen_fd = create_listen_socket(server_port);
    if (listen_fd < 0) return 1;
    printf("[SERVER OPT %d] Escuchando...\n", server_port);

    for (;;) {
        struct sockaddr_in cli;
        socklen_t clilen = sizeof(cli);
        int cfd = accept(listen_fd, (struct sockaddr *)&cli, &clilen);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(cfd);
            continue;
        }
        if (pid == 0) {
            // Proceso hijo
            close(listen_fd);
            handle_client(cfd, server_port);
        } else {
            // Proceso padre
            close(cfd);
        }
    }

    close(listen_fd);
    return 0;
}


