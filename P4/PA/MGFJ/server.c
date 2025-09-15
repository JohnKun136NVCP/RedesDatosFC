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

#define CTRL_PORT        49200     // Puerto de control
#define FIRST_DATA_PORT  49201     // Primer puerto de datos
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
    a.sin_addr.s_addr = bind_ip ? inet_addr(bind_ip) : htonl(INADDR_ANY);

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
    const char *BIND_IP = "192.168.0.1";           // IP local donde escuchar
    int ctrl_fd = bind_listen(CTRL_PORT, BIND_IP); // Socket de control
    fprintf(stderr, "[CTRL] Escuchando en %s:%d\n", BIND_IP ? BIND_IP : "0.0.0.0", CTRL_PORT);

    int next_data_port = FIRST_DATA_PORT;          // Ronda de puertos de datos

    for (;;) {
        // Acepta conexión de control y anuncia puerto de datos.
        struct sockaddr_in cli; socklen_t slen = sizeof(cli);
        int cfd = accept(ctrl_fd, (struct sockaddr*)&cli, &slen);
        if (cfd < 0) { perror("accept ctrl"); continue; }

        int data_port = next_data_port++;
        if (next_data_port > 65534 || next_data_port < FIRST_DATA_PORT)
            next_data_port = FIRST_DATA_PORT;

        char line[64];
        snprintf(line, sizeof(line), "PORT %d\n", data_port);
        send_all(cfd, line);
        close(cfd);

        // Abre el puerto de datos y atiende una sesión.
        int data_fd = bind_listen(data_port, BIND_IP);
        fprintf(stderr, "[DATA] Abierto puerto %d\n", data_port);

        struct sockaddr_in dcli; socklen_t dlen = sizeof(dcli);
        int d = accept(data_fd, (struct sockaddr*)&dcli, &dlen);
        if (d < 0) { perror("accept data"); close(data_fd); continue; }

        // Estados requeridos de la comunicación.
        send_all(d, "EN_ESPERA\n");
        send_all(d, "RECIBIENDO\n");

        // Lee todo lo que envíe el cliente (simulación de recepción de archivo).
        char buf[BUFSZ];
        ssize_t r;
        size_t total = 0;
        while ((r = recv(d, buf, sizeof(buf), 0)) > 0) {
            total += (size_t)r;
        }

        send_all(d, "TRANSMITIENDO\n");

        // Respuesta final con estadística.
        char ok[128];
        snprintf(ok, sizeof(ok), "OK bytes_recibidos=%zu\n", total);
        send_all(d, ok);

        close(d);
        close(data_fd);
        fprintf(stderr, "[DATA] Sesión en %d terminada (bytes=%zu)\n", data_port, total);
    }
    return 0;
}
