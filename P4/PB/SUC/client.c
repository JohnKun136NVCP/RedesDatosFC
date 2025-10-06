// PB client status: ejecuta consultas de estado a múltiples alias/hosts.
// Uso: client <SELF_ALIAS> <ALIAS1> <ALIAS2> <ALIAS3>
// Ejemplo: client s01 s02 s03 s04 (desde s01 consulta a s02, s03, s04).
//
// Protocolo (canal de control 49200):
//   1) Conectar a <host>:49200 y enviar "ASSIGN\n".
//   2) Recibir "PORT <p>\n".
//   3) Conectarse a <host>:<p>, enviar "STATUS\n" y leer respuesta.
// La salida agrega un timestamp y el host objetivo: "YYYY-MM-DD HH:MM:SS <host> <ESTADO>".

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CONTROL_PORT 49200
#define BUF 1024

// Conecta a (host, port) usando getaddrinfo (IPv4/IPv6)
static int connect_host_port(const char *host, int port) {
    char portstr[16];
    snprintf(portstr, sizeof(portstr), "%d", port);
    struct addrinfo hints, *res = NULL, *rp = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int err = getaddrinfo(host, portstr, &hints, &res);
    if (err != 0) {
        fprintf(stderr, "getaddrinfo %s: %s\n", host, gai_strerror(err));
        return -1;
    }
    int fd = -1;
    for (rp = res; rp; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

// Lee una línea del socket (hasta '\n' o EOF)
static int recv_line(int fd, char *buf, size_t max) {
    size_t t = 0;
    while (t + 1 < max) {
        char c;
        ssize_t r = recv(fd, &c, 1, 0);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) break;
        buf[t++] = c;
        if (c == '\n') break;
    }
    buf[t] = '\0';
    return (int)t;
}

// Genera timestamp local "YYYY-MM-DD HH:MM:SS"
static void now(char *out, size_t max) {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(out, max, "%Y-%m-%d %H:%M:%S", &tm);
}

// Consulta el estado a un host: solicita puerto efímero y luego envía STATUS
static void query_one(const char *self, const char *target) {
    (void)self;
    int cfd = connect_host_port(target, CONTROL_PORT);
    if (cfd < 0) {
        fprintf(stderr, "no connect %s\n", target);
        return;
    }
    const char *assign = "ASSIGN\n";
    send(cfd, assign, strlen(assign), 0);
    char line[BUF];
    if (recv_line(cfd, line, sizeof(line)) <= 0) {
        close(cfd);
        return;
    }
    int port = -1;
    if (sscanf(line, "PORT %d", &port) != 1) {
        close(cfd);
        return;
    }
    close(cfd);

    int dfd = connect_host_port(target, port);
    if (dfd < 0) return;
    const char *status = "STATUS\n";
    send(dfd, status, strlen(status), 0);
    if (recv_line(dfd, line, sizeof(line)) <= 0) {
        close(dfd);
        return;
    }
    close(dfd);
    char ts[32];
    now(ts, sizeof(ts));
    printf("%s %s %s", ts, target, line);
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Uso: %s <SELF_ALIAS> <ALIAS1> <ALIAS2> <ALIAS3>\n", argv[0]);
        return 1;
    }
    const char *self = argv[1];
    for (int i = 2; i < 5; i++) {
        query_one(self, argv[i]);
    }
    return 0;
}


