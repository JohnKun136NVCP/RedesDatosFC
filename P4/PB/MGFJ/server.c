#define _GNU_SOURCE
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>

#define CTRL_PORT 49200       // Primer puerto de datos
#define BACKLOG          16        // Cola de conexiones
#define BUFSZ            4096      // Tamaño de buffer para lectura

// Crea socket, hace bind a (port, bind_ip) y pone a escuchar.
static int bind_listen(int port, const char *bind_ip) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); exit(1); }

    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in a; memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port   = htons(port);

    if (!bind_ip) {
        a.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        struct addrinfo hints, *res = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(bind_ip, NULL, &hints, &res) != 0 || !res) {
            fprintf(stderr, "getaddrinfo failed for '%s'\n", bind_ip);
            exit(1);
        }
        a.sin_addr = ((struct sockaddr_in*)res->ai_addr)->sin_addr;
        freeaddrinfo(res);
    }

    if (bind(fd, (struct sockaddr*)&a, sizeof(a)) < 0) { perror("bind"); exit(1); }
    if (listen(fd, BACKLOG) < 0) { perror("listen"); exit(1); }
    return fd;
}

// Envía todo el contenido de la cadena s por el socket fd.
static void send_all(int fd, const char *s) {
    size_t n = strlen(s), off = 0;
    while (off < n) {
        ssize_t w = send(fd, s + off, n - off, 0);
        if (w <= 0) { perror("send"); return; }
        off += (size_t)w;
    }
}

int main(int argc, char **argv) {
    const char *ips[32];
    int n_ips = 0;

    if (argc <= 1) {
        ips[n_ips++] = NULL; // 0.0.0.0
    } else {
        for (int i = 1; i < argc && n_ips < 32; i++) ips[n_ips++] = argv[i];
    }

    int next_data_port = CTRL_PORT;
    fprintf(stderr, "[DATA] (Init) primer puerto %d en", next_data_port);
    for (int i = 0; i < n_ips; i++) fprintf(stderr, " %s", ips[i] ? ips[i] : "0.0.0.0");
    fprintf(stderr, "\n");

    for (;;) {
        int fds[32];
        struct pollfd pfds[32];
        for (int i = 0; i < n_ips; i++) {
            fds[i] = bind_listen(next_data_port, ips[i]);
            pfds[i].fd = fds[i];
            pfds[i].events = POLLIN;
            pfds[i].revents = 0;
        }

        fprintf(stderr, "[DATA] Abierto puerto %d en", next_data_port);
        for (int i = 0; i < n_ips; i++) fprintf(stderr, " %s", ips[i] ? ips[i] : "0.0.0.0");
        fprintf(stderr, "\n");

        int who = -1;
        for (;;) {
            int r = poll(pfds, n_ips, -1);
            if (r < 0) { if (errno == EINTR) continue; perror("poll"); break; }
            for (int i = 0; i < n_ips; i++) {
                if (pfds[i].revents & POLLIN) { who = i; break; }
            }
            if (who >= 0) break;
        }

        struct sockaddr_in dcli; socklen_t dlen = sizeof(dcli);
        int d = accept(fds[who], (struct sockaddr*)&dcli, &dlen);
        if (d < 0) perror("accept");

        for (int i = 0; i < n_ips; i++) close(fds[i]);
        if (d < 0) {
            next_data_port++;
            if (next_data_port > 65534) next_data_port = CTRL_PORT;
            continue;
        }

        // Estados requeridos de la comunicación.
        send_all(d, "EN_ESPERA\n");
        send_all(d, "RECIBIENDO\n");

        char buf[BUFSZ];
        ssize_t r;
        size_t total = 0;
        while ((r = recv(d, buf, sizeof(buf), 0)) > 0) {
            total += (size_t)r;
        }

        send_all(d, "TRANSMITIENDO\n");
        char ok[128];
        snprintf(ok, sizeof(ok), "OK bytes_recibidos=%zu\n", total);
        send_all(d, ok);

        close(d);
        fprintf(stderr, "[DATA] Sesión en %d terminada (bytes=%zu)\n", next_data_port, total);

        next_data_port++;
        if (next_data_port > 65534) next_data_port = CTRL_PORT;
    }
    return 0;
}
