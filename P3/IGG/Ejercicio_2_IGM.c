#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>     // <— para vsnprintf
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 8192

/* Envia texto formateado por socket (reemplaza dprintf) */
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
    if (!s) return;
    shift %= 26; if (shift < 0) shift += 26;
    for (char *p = s; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if (c >= 'a' && c <= 'z') *p = (char)('a' + ((c - 'a' + shift) % 26));
        else if (c >= 'A' && c <= 'Z') *p = (char)('A' + ((c - 'A' + shift) % 26));
    }
}

static ssize_t recv_all_once(int fd, char *buf, size_t cap) {
    ssize_t total = 0;
    while (total < (ssize_t)cap - 1) {
        ssize_t n = recv(fd, buf + total, cap - 1 - total, 0);
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        if (n == 0) break;
        total += n;
        if (n < 512) break; // heurística simple
    }
    buf[total] = '\0';
    return total;
}

static int serve_on_port(int my_port) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return -1; }
    int opt = 1; setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr; memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)my_port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); close(s); return -1; }
    if (listen(s, 8) < 0) { perror("listen"); close(s); return -1; }
    printf("Servidor escuchando en puerto %d\n", my_port);

    for (;;) {
        struct sockaddr_in cli; socklen_t clilen = sizeof(cli);
        int c = accept(s, (struct sockaddr*)&cli, &clilen);
        if (c < 0) { if (errno==EINTR) continue; perror("accept"); continue; }

        char buf[BUFFER_SIZE];
        ssize_t n = recv_all_once(c, buf, sizeof(buf));
        if (n <= 0) { close(c); continue; }

        // Espera: "<puerto_destino> <desplazamiento> <contenido>"
        char *p = buf, *endptr = NULL;
        long dst = strtol(p, &endptr, 10);
        if (endptr == p) { sendf(c, "RECHAZADO: Formato inválido (puerto_destino)\n"); close(c); continue; }
        p = endptr;
        long shift = strtol(p, &endptr, 10);
        if (endptr == p) { sendf(c, "RECHAZADO: Formato inválido (desplazamiento)\n"); close(c); continue; }
        p = endptr;
        while (*p == ' ') ++p;
        if (*p == '\0') { sendf(c, "RECHAZADO: Sin contenido\n"); close(c); continue; }

        if ((int)dst == my_port) {
            caesar_inplace(p, (int)shift);
            sendf(c, "PROCESADO: %s", p);
        } else {
            sendf(c, "RECHAZADO: Puerto incorrecto\n");
        }
        close(c);
    }
}

int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "Uso: %s <PUERTO>\n", argv[0]); return 1; }
    int port = atoi(argv[1]);
    return serve_on_port(port) == 0 ? 0 : 1;
}
