// serverOpy.c
// Para ajecutar use:  ./server 49200  

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BACKLOG 8
#define BUFSZ   4096

static void caesar(char *s, int shift) {
    shift = ((shift % 26) + 26) % 26;
    for (size_t i=0; s[i]; i++) {
        unsigned char c = (unsigned char)s[i];
        if (isupper(c)) s[i] = (char)(((c - 'A' + shift) % 26) + 'A');
        else if (islower(c)) s[i] = (char)(((c - 'a' + shift) % 26) + 'a');
    }
}

static int read_line(int fd, char *out, size_t cap) {
    size_t n = 0;
    while (n+1 < cap) {
        char c;
        ssize_t r = recv(fd, &c, 1, 0);
        if (r <= 0) return -1;
        out[n++] = c;
        if (c == '\n') break;
    }
    out[n] = '\0';
    return (int)n;
}

int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "Uso: %s <PUERTO>\n", argv[0]); return 1; }
    int my_port = atoi(argv[1]);

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return 1; }

    int yes=1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)my_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(s, BACKLOG) < 0) { perror("listen"); return 1; }

    printf("[server %d] escuchando...\n", my_port);

    for (;;) {
        int c = accept(s, NULL, NULL);
        if (c < 0) { perror("accept"); continue; }

        char header[256];
        if (read_line(c, header, sizeof(header)) < 0) { close(c); continue; }
        int target, shift, len;
        if (sscanf(header, "%d %d %d", &target, &shift, &len) != 3 || len < 0 || len > 1000000) {
            const char *bad = "REJECTED\n";
            send(c, bad, strlen(bad), 0);
            close(c);
            continue;
        }

        char *payload = malloc((size_t)len + 1);
        if (!payload) { close(c); continue; }
        size_t got = 0;
        while (got < (size_t)len) {
            ssize_t r = recv(c, payload + got, (size_t)len - got, 0);
            if (r <= 0) { break; }
            got += (size_t)r;
        }
        payload[got] = '\0';

        if (got == (size_t)len && target == my_port) {
            caesar(payload, shift);
            const char *ok = "PROCESSED\n";
            send(c, ok, strlen(ok), 0);
            send(c, payload, got, 0);
        } else {
            const char *rej = "REJECTED\n";
            send(c, rej, strlen(rej), 0);
        }

        free(payload);
        close(c);
    }
}
