#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define BACKLOG 16
#define RECV_BUF 8192

static volatile int busy = 0;

static int bind_listen_port(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = INADDR_ANY; // escucha en todos los alias
    a.sin_port = htons(port);
    if (bind(fd, (struct sockaddr*)&a, sizeof(a)) < 0) { close(fd); return -1; }
    if (listen(fd, BACKLOG) < 0) { close(fd); return -1; }
    return fd;
}

// Busca un puerto libre empezando en start retorna fd y pone puerto en *out_port
static int alloc_port_above(uint16_t start, uint16_t *out_port) {
    for (uint32_t p = start; p <= 60000; ++p) {
        int lfd = bind_listen_port((uint16_t)p);
        if (lfd >= 0) { *out_port = (uint16_t)p; return lfd; }
    }
    return -1;
}

static int accept_one(int lfd) {
    struct sockaddr_in ca; socklen_t cl = sizeof(ca);
    int c = accept(lfd, (struct sockaddr*)&ca, &cl);
    return c;
}

static ssize_t recv_line(int fd, char *buf, size_t max) {
    size_t i = 0; char c;
    while (i + 1 < max) {
        ssize_t r = recv(fd, &c, 1, 0);
        if (r <= 0) return r;
        if (c == '\n') { buf[i] = '\0'; return (ssize_t)i; }
        buf[i++] = c;
    }
    buf[i] = '\0';
    return (ssize_t)i;
}

static int ensure_dir(const char *subdir) {
    char path[512];
    const char *home = getenv("HOME"); if (!home) home = ".";
    snprintf(path, sizeof(path), "%s/%s", home, subdir);
    struct stat st;
    if (stat(path, &st) == -1) {
        if (mkdir(path, 0700) == -1) { perror("mkdir"); return -1; }
    }
    return 0;
}

static FILE *open_dst(const char *subdir, const char *name) {
    char path[512];
    const char *home = getenv("HOME"); if (!home) home = ".";
    snprintf(path, sizeof(path), "%s/%s/%s", home, subdir, name);
    return fopen(path, "wb");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "Uso: %s <port_base> <dir_servidor>\n", argv[0]); return 1; }
    uint16_t port_base = (uint16_t)atoi(argv[1]);
    const char *dir = argv[2]; 

    if (port_base == 0) { fprintf(stderr,"port_base no valido\n"); return 1; }
    if (ensure_dir(dir) != 0) return 1; 

    int lfd_base = bind_listen_port(port_base);
    if (lfd_base < 0) { perror("bind base"); return 1; }
    fprintf(stderr, "[SERVIDOR] escuchando base %u\n", port_base);

    char *bigbuf = malloc(1<<20);
    if (!bigbuf) { fprintf(stderr,"memoria\n"); close(lfd_base); return 1; }

    for (;;) {
        int c0 = accept_one(lfd_base);
        if (c0 < 0) { perror("accept base"); continue; }

        // asignar puerto > 49200 
        uint16_t eph = 0;
        int lfd_eph = alloc_port_above(49201, &eph);
        if (lfd_eph < 0) {
            fprintf(stderr,"No hay puerto efimero disponible\n");
            close(c0); continue;
        }

        char portmsg[64];
        int m = snprintf(portmsg, sizeof(portmsg), "PORT %u\n", eph);
        send(c0, portmsg, (size_t)m, 0);
        close(c0);

        int c1 = accept_one(lfd_eph);
        close(lfd_eph);
        if (c1 < 0) { perror("accept eph"); continue; }

        // identificar alias local al que llegó c1 y preparar directorio
        struct sockaddr_in local_addr; socklen_t local_len = sizeof(local_addr);
        memset(&local_addr, 0, sizeof(local_addr));
        const char *alias_dir = "desconocido";
        if (getsockname(c1, (struct sockaddr*)&local_addr, &local_len) == 0 &&
            local_addr.sin_family == AF_INET) {
            uint32_t ip = ntohl(local_addr.sin_addr.s_addr);

          
            if (ip == 0xC0A80165u) alias_dir = "s01"; // 192.168.1.101
            else if (ip == 0xC0A80166u) alias_dir = "s02"; // 192.168.1.102
            else if (ip == 0xC0A80167u) alias_dir = "s03"; // 192.168.1.103
            else if (ip == 0xC0A80168u) alias_dir = "s04"; // 192.168.1.104
        }
        ensure_dir(alias_dir);

        // leer primera linea
        char line[1024];
        ssize_t n = recv_line(c1, line, sizeof(line));
        if (n <= 0) { close(c1); continue; }

        if (strncmp(line, "STATUS", 6) == 0) {
            // muestra recibiendo cuando está ocupado
            const char *resp = busy ? "RECIBIENDO\n" : "ESPERA\n";
            send(c1, resp, strlen(resp), 0);
            close(c1);
            continue;
        }

        if (strncmp(line, "PUT ", 4) == 0) {
            char fname[256] = {0};
            sscanf(line + 4, "%255s", fname);
            if (recv_line(c1, line, sizeof(line)) <= 0) { close(c1); continue; }
            long long total = atoll(line);
            if (total < 0) total = 0;

            // guardar según el alias que recibió la conexión
            FILE *out = open_dst(alias_dir, fname);
            if (!out) { close(c1); continue; }

            busy = 1;
            long long got = 0;
            while (got < total) {
                ssize_t r = recv(c1, bigbuf, RECV_BUF, 0);
                if (r <= 0) break;
                fwrite(bigbuf, 1, (size_t)r, out);
                got += r;
            }
            fclose(out);
            busy = 0;
            send(c1, "OK\n", 3, 0);
            close(c1);
            continue;
        }

        // desconocido
        close(c1);
    }

    free(bigbuf);
    close(lfd_base);
    return 0;
}
