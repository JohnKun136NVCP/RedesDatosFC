// server.c
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static const char* STATUS_WAITING = "WAITING\n";
static const char* STATUS_RECEIVING = "RECEIVING\n";
static volatile sig_atomic_t busy = 0;

static void die(const char* m) { perror(m); exit(1); }

static int bind_listen_ipv4(const char* ip, int port) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) die("socket");
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in a = { 0 };
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &a.sin_addr) != 1) die("inet_pton bind_ip");
    if (bind(s, (struct sockaddr*)&a, sizeof(a)) < 0) die("bind");
    if (listen(s, 16) < 0) die("listen");
    return s;
}

static int accept_one(int ls) {
    struct sockaddr_in c; socklen_t cl = sizeof(c);
    int s = accept(ls, (struct sockaddr*)&c, &cl);
    return s;
}

static ssize_t read_line(int s, char* buf, size_t n) {
    size_t i = 0;
    while (i + 1 < n) {
        char c; ssize_t r = recv(s, &c, 1, 0);
        if (r <= 0) return r;
        if (c == '\n') { buf[i] = '\0'; return (ssize_t)i; }
        buf[i++] = c;
    }
    buf[i] = '\0'; return (ssize_t)i;
}

static char* basename_str(char* path) {
    char* b = strrchr(path, '/');
    return b ? b + 1 : path;
}

static off_t recv_all_to_file(int s, const char* path, off_t expected) {
    int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd < 0) die("open file");
    off_t recvd = 0; char buf[1 << 15];
    while (expected < 0 || recvd < expected) {
        ssize_t want = sizeof(buf);
        if (expected >= 0) {
            off_t left = expected - recvd;
            if (left < want) want = (ssize_t)left;
        }
        ssize_t r = recv(s, buf, want, 0);
        if (r < 0) { perror("recv data"); break; }
        if (r == 0) break;
        ssize_t w = write(fd, buf, r);
        if (w != r) { perror("write file"); break; }
        recvd += r;
    }
    close(fd);
    return recvd;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "Uso: %s <bind_ip> <control_port> <dest_dir>\n", argv[0]);
        return 1;
    }
    const char* bind_ip = argv[1];
    int control_port = atoi(argv[2]);
    const char* dest_dir = argv[3];
    if (access(dest_dir, W_OK) != 0) { fprintf(stderr, "Directorio no accesible: %s\n", dest_dir); return 1; }

    int ls = bind_listen_ipv4(bind_ip, control_port);
    printf("[server %s:%d] listo.\n", bind_ip, control_port);

    for (;;) {
        int cs = accept_one(ls);
        if (cs < 0) { perror("accept"); continue; }

        // Lee una línea de comando
        char line[1024] = { 0 };
        ssize_t n = read_line(cs, line, sizeof(line));
        if (n <= 0) { close(cs); continue; }

        if (strncmp(line, "STATUS", 6) == 0) {
            const char* resp = busy ? STATUS_RECEIVING : STATUS_WAITING;
            send(cs, resp, strlen(resp), 0);
            close(cs);
            continue;
        }

        if (strncmp(line, "FILE ", 5) == 0) {
            // Espera: "FILE <nombre> <size>"
            char name[512] = { 0 };
            long long size = -1;
            if (sscanf(line + 5, "%511s %lld", name, &size) != 2) { close(cs); continue; }
            char* base = basename_str(name);

            // Abrimos listener de datos en puerto efímero
            int dls = bind_listen_ipv4(bind_ip, 0);
            struct sockaddr_in sa; socklen_t sl = sizeof(sa);
            if (getsockname(dls, (struct sockaddr*)&sa, &sl) < 0) die("getsockname");
            int data_port = ntohs(sa.sin_port);

            // Enviamos al cliente el puerto
            char portline[64];
            snprintf(portline, sizeof(portline), "PORT %d\n", data_port);
            send(cs, portline, strlen(portline), 0);
            close(cs);

            busy = 1;
            int ds = accept_one(dls);
            close(dls);

            // Construye ruta destino
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", dest_dir, base);

            off_t got = recv_all_to_file(ds, path, size);
            close(ds);
            busy = 0;

            printf("Recibido %s (%lld bytes solicitados, %lld bytes recibidos)\n",
                base, (long long)size, (long long)got);
            continue;
        }

        // Comando desconocido
        close(cs);
    }
    return 0;
}
