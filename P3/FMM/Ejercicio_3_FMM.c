// serverOpt.c
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BACKLOG 16

static void caesar_inplace(char *s, int shift) {
    shift %= 26;
    if (shift < 0) shift += 26;
    for (size_t i = 0; s[i]; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (isupper(c)) s[i] = (char)(((c - 'A' + shift) % 26) + 'A');
        else if (islower(c)) s[i] = (char)(((c - 'a' + shift) % 26) + 'a');
    }
}

static ssize_t recvall(int fd, void *buf, size_t len) {
    size_t got = 0;
    while (got < len) {
        ssize_t r = recv(fd, (char*)buf + got, len - got, 0);
        if (r == 0) return (ssize_t)got;
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        got += (size_t)r;
    }
    return (ssize_t)got;
}
static ssize_t sendall(int fd, const void *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t r = send(fd, (const char*)buf + sent, len - sent, 0);
        if (r <= 0) { if (r < 0 && errno == EINTR) continue; return -1; }
        sent += (size_t)r;
    }
    return (ssize_t)sent;
}

struct _attribute_((packed)) Header {
    uint32_t target_port_be; // puerto objetivo
    int32_t  shift_be;       // desplazamiento César
    uint32_t data_len_be;    // longitud del texto
};

static int make_listener(int port) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); exit(1); }
    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in a; memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons((uint16_t)port);
    if (bind(s, (struct sockaddr*)&a, sizeof(a)) < 0) { perror("bind"); exit(1); }
    if (listen(s, BACKLOG) < 0) { perror("listen"); exit(1); }
    return s;
}

static void handle_client(int c, int listen_port) {
    struct Header h;
    if (recvall(c, &h, sizeof(h)) != (ssize_t)sizeof(h)) { close(c); return; }
    uint32_t target_port = ntohl(h.target_port_be);
    int32_t  shift       = ntohl(h.shift_be);
    uint32_t n           = ntohl(h.data_len_be);

    char buf = (char)malloc(n + 1);
    if (!buf) { close(c); return; }
    if (recvall(c, buf, n) != (ssize_t)n) { free(buf); close(c); return; }
    buf[n] = '\0';

    uint32_t resp = 0;
    if ((int)target_port == listen_port) {
        caesar_inplace(buf, shift);

        // nombre de salida por puerto
        char outname[64];
        snprintf(outname, sizeof(outname), "file_%d_cesar.txt", listen_port);
        FILE *fp = fopen(outname, "w");
        if (fp) { fputs(buf, fp); fclose(fp); }

        printf("[Puerto %d] Archivo recibido y cifrado como %s\n", listen_port, outname);
        resp = 1;
    } else {
        printf("[Servidor %d] Petición para puerto %u: RECHAZADA\n",
               listen_port, target_port);
    }

    uint32_t resp_be = htonl(resp);
    sendall(c, &resp_be, sizeof(resp_be));
    free(buf);
    close(c);
}

int main(void) {
    int ports[3] = {49200, 49201, 49202};
    int ls[3];
    for (int i = 0; i < 3; ++i) {
        ls[i] = make_listener(ports[i]);
        printf("[Servidor] Escuchando puerto %d\n", ports[i]);
    }

    fd_set rfds;
    int maxfd = ls[0]; if (ls[1] > maxfd) maxfd = ls[1]; if (ls[2] > maxfd) maxfd = ls[2];

    for (;;) {
        FD_ZERO(&rfds);
        for (int i = 0; i < 3; ++i) FD_SET(ls[i], &rfds);

        int r = select(maxfd + 1, &rfds, NULL, NULL, NULL);
        if (r < 0) { if (errno == EINTR) continue; perror("select"); break; }

        for (int i = 0; i < 3; ++i) {
            if (FD_ISSET(ls[i], &rfds)) {
                int c = accept(ls[i], NULL, NULL);
                if (c >= 0) handle_client(c, ports[i]);
            }
        }
    }

    for (int i = 0; i < 3; ++i) close(ls[i]);
    return 0;
}
