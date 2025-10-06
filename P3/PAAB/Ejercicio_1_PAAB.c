#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define BUFFER_SIZE 1024

// César 
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

int main(void) {
    int ports[3] = {49200, 49201, 49202};
    int listenfd[3] = {-1, -1, -1};
    int maxfd = -1;

    // Crear, bind y listen en los tres puertos
    for (int i = 0; i < 3; i++) {
        listenfd[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (listenfd[i] < 0) { perror("socket"); return 1; }
        int opt = 1; setsockopt(listenfd[i], SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in addr; memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(ports[i]);

        if (bind(listenfd[i], (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("bind"); return 1;
        }
        if (listen(listenfd[i], 8) < 0) {
            perror("listen"); return 1;
        }
        if (listenfd[i] > maxfd) maxfd = listenfd[i];
        printf("[*][SERVER %d] LISTENING...\n", ports[i]);
    }

    // Bucle principal con select()
    while (1) {
        fd_set rfds; FD_ZERO(&rfds);
        for (int i = 0; i < 3; i++) FD_SET(listenfd[i], &rfds);

        int ready = select(maxfd + 1, &rfds, NULL, NULL, NULL);
        if (ready < 0) { perror("select"); continue; }

        for (int i = 0; i < 3; i++) {
            if (!FD_ISSET(listenfd[i], &rfds)) continue;

            struct sockaddr_in caddr; socklen_t clen = sizeof(caddr);
            int cli = accept(listenfd[i], (struct sockaddr *)&caddr, &clen);
            if (cli < 0) { perror("accept"); continue; }

            // Protocolo: TARGET_PORT, SHIFT, LEN, blank, cuerpo
            char line[BUFFER_SIZE];
            int target = 0, shift = 0, len = 0;

            if (read_line(cli, line, sizeof line) <= 0) { close(cli); continue; }
            sscanf(line, "TARGET_PORT:%d", &target);
            if (read_line(cli, line, sizeof line) <= 0) { close(cli); continue; }
            sscanf(line, "SHIFT:%d", &shift);
            if (read_line(cli, line, sizeof line) <= 0) { close(cli); continue; }
            sscanf(line, "LEN:%d", &len);
            if (read_line(cli, line, sizeof line) <= 0) { close(cli); continue; } // blank

            if (len <= 0 || len > (1<<20)) { // 1 MB máx
                const char *rej = "RECHAZADO\n";
                send(cli, rej, strlen(rej), 0);
                close(cli);
                continue;
            }

            char *body = (char*)malloc(len + 1);
            if (!body) { close(cli); continue; }
            if (read_n(cli, body, len) != len) { free(body); close(cli); continue; }
            body[len] = '\0';

            int local_port = ports[i]; // sabemos cuál puerto por el listen que disparó

            if (target == local_port) {
                printf("[*][SERVER %d] Request accepted...\n", local_port);
                encryptCaesar(body, shift);
                printf("[*][SERVER %d] File received and encrypted: %s\n", local_port, body);

                char hdr[64];
                int n = snprintf(hdr, sizeof hdr, "PROCESADO\nLEN:%d\n\n", len);
                send(cli, hdr, n, 0);
                send(cli, body, len, 0);
            } else {
                const char *rej = "RECHAZADO\n";
                send(cli, rej, strlen(rej), 0);
                printf("[*][SERVER %d] Request rejected (target=%d)\n", local_port, target);
            }

            free(body);
            close(cli);
        }
    }

    for (int i = 0; i < 3; i++) close(listenfd[i]);
    return 0;
}
