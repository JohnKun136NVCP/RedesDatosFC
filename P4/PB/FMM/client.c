// clientPB.c
// Usos:
//   ./clientPB <HOST|ALIAS> <PORT> <file>
// Modo status (alias se conectan a los otros 3):
//   ./clientPB --status <alias:s01|s02|s03|s04> <PORT> <file>

#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFSZ 4096

static int connect_host_port(const char *host, const char *port) {
    struct addrinfo hints, *res = NULL, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int rc = getaddrinfo(host, port, &hints, &res);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo(%s:%s): %s\n", host, port, gai_strerror(rc));
        return -1;
    }
    int s = -1;
    for (p = res; p; p = p->ai_next) {
        s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s < 0) continue;
        if (connect(s, p->ai_addr, p->ai_addrlen) == 0) break;
        close(s); s = -1;
    }
    freeaddrinfo(res);
    return s;
}

static int send_file(int s, FILE *f, size_t *bytes_out) {
    char buf[BUFSZ];
    size_t total = 0;
    for (;;) {
        size_t n = fread(buf, 1, sizeof(buf), f);
        if (n == 0) break;
        size_t sent = 0;
        while (sent < n) {
            ssize_t w = send(s, buf + sent, n - sent, 0);
            if (w <= 0) {
                if (errno == EINTR) continue;
                perror("send");
                return -1;
            }
            sent  += (size_t)w;
            total += (size_t)w;
        }
    }
    if (bytes_out) *bytes_out = total;
    return 0;
}

static int send_to_server(const char *host, const char *port, const char *file) {
    FILE *f = fopen(file, "rb");
    if (!f) { perror("file"); return -1; }

    int s = connect_host_port(host, port);
    if (s < 0) { fclose(f); return -1; }

    size_t total = 0;
    int rc = send_file(s, f, &total);
    fclose(f);
    close(s);

    if (rc == 0) {
        printf("[CLIENT] Enviado %zu bytes a %s:%s\n", total, host, port);
        return 0;
    }
    return -1;
}

static int status_mode(const char *id, const char *port, const char *file) {
    const char *all[4] = {"s01","s02","s03","s04"};
    printf("[CLIENT] status '%s' conectando...\n", id);

    for (int i = 0; i < 4; i++) {
        if (strcmp(all[i], id) == 0) continue;
        int ok = (send_to_server(all[i], port, file) == 0);
        printf("[CLIENT] %s -> %s : %s\n", id, all[i], ok ? "OK" : "FAIL");
    }
    return 0;
}

int main(int argc, char **argv) {
    // Modo status
    if (argc == 5 && strcmp(argv[1], "--status") == 0) {
        const char *id   = argv[2];           // s01|s02|s03|s04
        const char *port = argv[3];
        const char *file = argv[4];
        return status_mode(id, port, file);
    }

    // Manda desde un alias un archivo a los demás
    if (argc != 4) {
        fprintf(stderr,
            "Usos:\n"
            "  %s <HOST|ALIAS> <PORT> <file>\n"
            "  %s --status <alias:s01|s02|s03|s04> <PORT> <file>\n",
            argv[0], argv[0]);
        return 1;
    }

    const char *host = argv[1];
    const char *port = argv[2];
    const char *file = argv[3];

    return (send_to_server(host, port, file) == 0) ? 0 : 1;
}