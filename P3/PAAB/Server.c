#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define PORT 7006       
#define BUFFER_SIZE 1024 

// Cesar
static void encryptCaesar(char *text, int shift) {
    int k = ((shift % 26) + 26) % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        unsigned char c = text[i];
        if (c >= 'A' && c <= 'Z') text[i] = (char)(((c - 'A' + k) % 26) + 'A');
        else if (c >= 'a' && c <= 'z') text[i] = (char)(((c - 'a' + k) % 26) + 'a');
    }
}

// Lectura
static int read_line(int fd, char *buf, int max) {
    int i = 0;
    while (i < max - 1) {
        char c;
        int r = recv(fd, &c, 1, 0);
        if (r <= 0) return r;
        buf[i++] = c;
        if (c == '\n') break;
    }
    buf[i] = '\0';
    return i;
}
static int read_n(int fd, char *buf, int n) {
    int got = 0;
    while (got < n) {
        int r = recv(fd, buf + got, n - got, 0);
        if (r <= 0) return r;
        got += r;
    }
    return got;
}

int main(int argc, char **argv) {
    int my_port = (argc == 2) ? atoi(argv[1]) : PORT;

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) { perror("socket"); return 1; }

    int opt = 1; setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr; memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(my_port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind"); close(server_sock); return 1;
    }
    if (listen(server_sock, 8) < 0) {
        perror("listen"); close(server_sock); return 1;
    }

    printf("[*][SERVER %d] LISTENING...\n", my_port);

    while (1) {
        struct sockaddr_in client_addr; socklen_t addr_size = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
        if (client_sock < 0) { perror("accept"); continue; }

        char line[BUFFER_SIZE];
        int target = 0, shift = 0, len = 0;

        if (read_line(client_sock, line, sizeof line) <= 0) { close(client_sock); continue; }
        sscanf(line, "TARGET_PORT:%d", &target);
        if (read_line(client_sock, line, sizeof line) <= 0) { close(client_sock); continue; }
        sscanf(line, "SHIFT:%d", &shift);
        if (read_line(client_sock, line, sizeof line) <= 0) { close(client_sock); continue; }
        sscanf(line, "LEN:%d", &len);
        if (read_line(client_sock, line, sizeof line) <= 0) { close(client_sock); continue; } // blank

        if (len <= 0 || len > (1<<20)) {
            const char *rej = "RECHAZADO\n";
            send(client_sock, rej, strlen(rej), 0);
            close(client_sock);
            continue;
        }

        char *body = (char *)malloc(len + 1);
        if (!body) { close(client_sock); continue; }
        if (read_n(client_sock, body, len) != len) { free(body); close(client_sock); continue; }
        body[len] = '\0';

        if (target == my_port) {
            printf("[*][SERVER %d] Request accepted...\n", my_port);
            encryptCaesar(body, shift);
            printf("[*][SERVER %d] File received and encrypted: %s\n", my_port, body);

            char hdr[64];
            int n = snprintf(hdr, sizeof hdr, "PROCESADO\nLEN:%d\n\n", len);
            send(client_sock, hdr, n, 0);
            send(client_sock, body, len, 0);
        } else {
            const char *rej = "RECHAZADO\n";
            send(client_sock, rej, strlen(rej), 0);
            printf("[*][SERVER %d] Request rejected (target=%d)\n", my_port, target);
        }

        free(body);
        close(client_sock);
    }

    close(server_sock);
    return 0;
}
