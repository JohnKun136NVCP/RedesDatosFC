#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>     
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 8192
static const int PORTS[3] = {49200, 49201, 49202};

static int sendf(int fd, const char *fmt, ...) {
    char out[BUFFER_SIZE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(out, sizeof(out), fmt, ap);
    va_end(ap);
    size_t len = strlen(out);
    return (int)send(fd, out, len, 0);
}

static void caesar_inplace(char *s, int shift) {
    shift %= 26; if (shift < 0) shift += 26;
    for (char *p = s; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if (c >= 'a' && c <= 'z') *p = (char)('a' + ((c - 'a' + shift) % 26));
        else if (c >= 'A' && c <= 'Z') *p = (char)('A' + ((c - 'A' + shift) % 26));
    }
}

static int listen_on(int port) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return -1; }
    int opt = 1; setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr; memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); close(s); return -1; }
    if (listen(s, 16) < 0) { perror("listen"); close(s); return -1; }
    printf("Escuchando en %d\n", port);
    return s;
}

static void handle_client(int c, int my_port) {
    char buf[BUFFER_SIZE];
    ssize_t n = recv(c, buf, sizeof(buf)-1, 0);
    if (n <= 0) { close(c); return; }
    buf[n] = '\0';

    char *p = buf, *end = NULL;
    long dst   = strtol(p,   &end, 10);
    if (end == p) { sendf(c, "RECHAZADO: Formato inválido (puerto)\n"); close(c); return; }
    p = end;
    long shift = strtol(p,   &end, 10);
    if (end == p) { sendf(c, "RECHAZADO: Formato inválido (shift)\n");  close(c); return; }
    p = end;
    while (*p == ' ') ++p;
    if (*p == '\0') { sendf(c, "RECHAZADO: Sin contenido\n"); close(c); return; }

    if ((int)dst == my_port) {
        caesar_inplace(p, (int)shift);
        sendf(c, "PROCESADO: %s", p);
    } else {
        sendf(c, "RECHAZADO: Puerto incorrecto\n");
    }
    close(c);
}

int main(void) {
    int ls[3];
    for (int i = 0; i < 3; ++i) {
        ls[i] = listen_on(PORTS[i]);
        if (ls[i] < 0) return 1;
    }
    struct pollfd pfds[3];
    for (int i = 0; i < 3; ++i) {
        pfds[i].fd = ls[i];
        pfds[i].events = POLLIN;
    }

    for (;;) {
        int ready = poll(pfds, 3, -1);
        if (ready < 0) {
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }
        for (int i = 0; i < 3; ++i) {
            if (pfds[i].revents & POLLIN) {
                struct sockaddr_in cli; socklen_t cl = sizeof(cli);
                int c = accept(ls[i], (struct sockaddr*)&cli, &cl);
                if (c < 0) { perror("accept"); continue; }
                handle_client(c, PORTS[i]);
            }
        }
    }

   
    for (int i = 0; i < 3; ++i) {
        close(ls[i]);
    }
    return 0;
}
