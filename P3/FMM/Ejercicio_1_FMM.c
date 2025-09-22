// Server.c
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BACKLOG 16
#define OUTFILE "info.txt"

static ssize_t recvall(int fd, void *buf, size_t len) {
    uint8_t *p = buf;
    size_t got = 0;
    while (got < len) {
        ssize_t r = recv(fd, p + got, len - got, 0);
        if (r == 0) {
            return got; // conexión cerrada
        }
        if (r < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        got += (size_t)r;
    }
    return (ssize_t)got;
}

static ssize_t sendall(int fd, const void *buf, size_t len) {
    const uint8_t *p = buf;
    size_t sent = 0;
    while (sent < len) {
        ssize_t r = send(fd, p + sent, len - sent, 0);
        if (r <= 0) {
            if (r < 0 && errno == EINTR) {
                continue;
            }
            return -1;
        }
        sent += (size_t)r;
    }
    return (ssize_t)sent;
}

static void caesar_inplace(char *s, int shift) {
    shift = shift % 26;
    if (shift < 0) {
        shift += 26;
    }
    for (size_t i = 0; s[i]; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (isupper(c)) {
            s[i] = (char)(((c - 'A' + shift) % 26) + 'A');
        }
        else if (islower(c)) {
            s[i] = (char)(((c - 'a' + shift) % 26) + 'a');
        }
    }
}

struct _attribute_((packed)) Header {
    uint32_t target_port_be; // puerto objetivo
    int32_t  shift_be;       // desplazamiento
    uint32_t data_len_be;    // longitud del texto
};

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <PUERTO>\n", argv[0]);
        return 1;
    }

    int listen_port = atoi(argv[1]);

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)listen_port);

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(s, BACKLOG) < 0) {
        perror("listen");
        return 1;
    }

    printf("[*][SERVER %d] Listening...\n", listen_port);

    for (;;) {
        int c = accept(s, NULL, NULL);
        if (c < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            break;
        }

        struct Header h;
        if (recvall(c, &h, sizeof(h)) != (ssize_t)sizeof(h)) {
            close(c);
            continue;
        }

        uint32_t target_port = ntohl(h.target_port_be);
        int32_t  shift       = ntohl(h.shift_be);
        uint32_t n           = ntohl(h.data_len_be);

        char *buf = malloc(n + 1);
        if (!buf) {
            close(c);
            continue;
        }

        if (recvall(c, buf, n) != (ssize_t)n) {
            free(buf);
            close(c);
            continue;
        }
        buf[n] = '\0';

        uint32_t resp = 0; // 0 = rechazado, 1 = procesado

        if ((int)target_port == listen_port) {
            caesar_inplace(buf, shift);

            FILE *fp = fopen(OUTFILE, "w");
            if (fp) {
                fputs(buf, fp);
                fclose(fp);
            }

            resp = 1;

            printf("[*][SERVER %d] Request accepted...\n", listen_port);
            printf("[*][SERVER %d] File received and encrypted:\n%s\n",
                   listen_port, buf);
        }
        else {
            printf("[*][SERVER %d] Request rejected (client requested port %u)\n",
                   listen_port, target_port);
        }

        uint32_t resp_be = htonl(resp);
        sendall(c, &resp_be, sizeof(resp_be));
        free(buf);
        close(c);
    }

    close(s);
    return 0;
}
