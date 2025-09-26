// server.c — Práctica III (con DEBUG)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#define BACKLOG 16
#define HDR_MAX 512

static void caesar(char *s, int shift) {
    shift %= 26;
    for (int i = 0; s[i]; ++i) {
        unsigned char c = s[i];
        if (c >= 'A' && c <= 'Z') s[i] = 'A' + (c - 'A' + shift + 26) % 26;
        else if (c >= 'a' && c <= 'z') s[i] = 'a' + (c - 'a' + shift + 26) % 26;
    }
}

static ssize_t recv_line(int fd, char *out, size_t max) {
    size_t pos = 0;
    while (pos + 1 < max) {
        char c;
        ssize_t r = recv(fd, &c, 1, 0);
        if (r == 0) return 0;
        if (r < 0) return -1;
        out[pos++] = c;
        if (c == '\n') break;
    }
    out[pos] = '\0';
    return (ssize_t)pos;
}

static int recv_n(int fd, char *buf, size_t n) {
    size_t got = 0;
    while (got < n) {
        ssize_t r = recv(fd, buf + got, n - got, 0);
        if (r == 0) return 0;
        if (r < 0) return -1;
        got += (size_t)r;
    }
    return 1;
}

int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "Uso: %s <PUERTO>\n", argv[0]); return 1; }
    int listen_port = atoi(argv[1]);

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return 1; }

    int opt = 1; setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(listen_port);

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(s, BACKLOG) < 0) { perror("listen"); return 1; }

    printf("[*] [SERVER %d] LISTENING...\n", listen_port); fflush(stdout);

    for (;;) {
        struct sockaddr_in cli; socklen_t clilen = sizeof(cli);
        int c = accept(s, (struct sockaddr*)&cli, &clilen);
        if (c < 0) { perror("accept"); continue; }

        char ip[64]; inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof(ip));
        printf("[*] [SERVER %d] Client %s:%d connected\n", listen_port, ip, ntohs(cli.sin_port)); fflush(stdout);

        char hdr[HDR_MAX];
        ssize_t hl = recv_line(c, hdr, sizeof(hdr));
        if (hl <= 0) {
            printf("[!] [SERVER %d] recv_line fallo (hl=%zd)\n", listen_port, hl); fflush(stdout);
            send(c, "ERROR\n", 6, 0);
            close(c); continue;
        }
        printf("[DEBUG %d] HDR='%s'\n", listen_port, hdr); fflush(stdout);

        int req_port = 0, shift = 0; size_t nbytes = 0;
        if (sscanf(hdr, "REQPORT %d SHIFT %d SIZE %zu", &req_port, &shift, &nbytes) != 3) {
            printf("[!] [SERVER %d] Header invalido\n", listen_port); fflush(stdout);
            send(c, "REJECTED\n", 9, 0);
            close(c); continue;
        }

        char *content = malloc(nbytes + 1);
        if (!content) {
            printf("[!] [SERVER %d] malloc fallo (%zu bytes)\n", listen_port, nbytes); fflush(stdout);
            send(c, "ERROR\n", 6, 0);
            close(c); continue;
        }

        int rn = recv_n(c, content, nbytes);
        if (rn != 1) {
            printf("[!] [SERVER %d] recv_n fallo (rn=%d)\n", listen_port, rn); fflush(stdout);
            send(c, "ERROR\n", 6, 0);
            free(content); close(c); continue;
        }
        content[nbytes] = '\0';

        if (req_port == listen_port) {
            caesar(content, shift);
            printf("[+] [SERVER %d] ACCEPTED. Preview: %.60s\n", listen_port, content); fflush(stdout);
            send(c, "ACCEPTED\n", 9, 0);
        } else {
            printf("[-] [SERVER %d] REJECTED (client requested %d)\n", listen_port, req_port); fflush(stdout);
            send(c, "REJECTED\n", 9, 0);
        }

        free(content);
        close(c);
    }
    return 0;
}
