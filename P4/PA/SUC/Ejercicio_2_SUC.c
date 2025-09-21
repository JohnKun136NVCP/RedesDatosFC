// ClientStatus_PA: se conecta al servidor en el puerto de control (49200)
// Protocolo:
//   1) Conectar a sXX:49200
//   2) Enviar "ASSIGN\n" y recibir "PORT <p>\n"
//   3) Conectar a <p> y enviar "STATUS\n"; imprimir respuesta
// Además registra a stdout la línea: fecha, hora, estado

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

static int connect_host_port(const char *host, int port) {
    char portstr[16]; snprintf(portstr, sizeof(portstr), "%d", port);
    struct addrinfo hints, *res = NULL, *rp = NULL; memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
    int err = getaddrinfo(host, portstr, &hints, &res);
    if (err != 0) { fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err)); return -1; }
    int fd = -1;
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(fd); fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

static int recv_line(int fd, char *buf, size_t max) {
    size_t t = 0; while (t + 1 < max) {
        char c; ssize_t r = recv(fd, &c, 1, 0);
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        if (r == 0) break; buf[t++] = c; if (c == '\n') break;
    }
    buf[t] = '\0'; return (int)t;
}

static void now_string(char *out, size_t max) {
    time_t t = time(NULL); struct tm tm; localtime_r(&t, &tm);
    strftime(out, max, "%Y-%m-%d %H:%M:%S", &tm);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <host_alias>\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s s01\n", argv[0]);
        return 1;
    }
    const char *host = argv[1];

    int cfd = connect_host_port(host, CONTROL_PORT);
    if (cfd < 0) return 1;
    const char *assign = "ASSIGN\n"; send(cfd, assign, strlen(assign), 0);
    char line[BUF]; if (recv_line(cfd, line, sizeof(line)) <= 0) { close(cfd); return 1; }
    int port = -1; if (sscanf(line, "PORT %d", &port) != 1 || port <= 0) { close(cfd); return 1; }
    close(cfd);

    int dfd = connect_host_port(host, port);
    if (dfd < 0) return 1;
    const char *status = "STATUS\n"; send(dfd, status, strlen(status), 0);
    if (recv_line(dfd, line, sizeof(line)) <= 0) { close(dfd); return 1; }
    close(dfd);

    char ts[32]; now_string(ts, sizeof(ts));
    // Imprime: fecha, hora, estado
    // También podrías redirigir a archivo desde la shell: ./ClientStatus_PA s01 >> status_s01.log
    printf("%s %s", ts, line);
    return 0;
}


