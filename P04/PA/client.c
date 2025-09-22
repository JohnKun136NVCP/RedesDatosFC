// client.c
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

static void die(const char* m) { perror(m); exit(1); }

static int connect_host_port(const char* host, int port) {
    struct addrinfo hints = { 0 }, * res = 0;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char portstr[16]; snprintf(portstr, sizeof(portstr), "%d", port);
    int e = getaddrinfo(host, portstr, &hints, &res);
    if (e) { fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(e)); exit(1); }
    int s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s < 0) die("socket");
    if (connect(s, res->ai_addr, res->ai_addrlen) < 0) die("connect");
    freeaddrinfo(res);
    return s;
}

static char* basename_str(char* p) { char* b = strrchr(p, '/'); return b ? b + 1 : p; }

static ssize_t read_line(int s, char* buf, size_t n) {
    size_t i = 0; char c;
    while (i + 1 < n) {
        ssize_t r = recv(s, &c, 1, 0); if (r <= 0) return r;
        if (c == '\n') { buf[i] = '\0'; return (ssize_t)i; }
        buf[i++] = c;
    }
    buf[i] = '\0'; return (ssize_t)i;
}

int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "Uso: %s <server_host> <control_port> <archivo>\n", argv[0]); return 1; }
    const char* host = argv[1];
    int cport = atoi(argv[2]);
    char* filepath = argv[3];
    struct stat st;
    if (stat(filepath, &st) < 0) { perror("stat"); return 1; }
    long long size = st.st_size;
    char* base = basename_str(filepath);

    // 1) Control
    int cs = connect_host_port(host, cport);

    // 2) FILE <nombre> <size>
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "FILE %s %lld\n", base, size);
    send(cs, cmd, strlen(cmd), 0);

    // 3) Espera "PORT n"
    char line[128] = { 0 };
    if (read_line(cs, line, sizeof(line)) <= 0) { fprintf(stderr, "No PORT\n"); close(cs); return 1; }
    close(cs);
    int dport = 0;
    if (sscanf(line, "PORT %d", &dport) != 1) { fprintf(stderr, "Linea inesperada: %s\n", line); return 1; }

    // 4) Datos
    int ds = connect_host_port(host, dport);
    int fd = open(filepath, O_RDONLY);
    if (fd < 0) die("open");

    char buf[1 << 15];
    long long sent = 0;
    for (;;) {
        ssize_t r = read(fd, buf, sizeof(buf));
        if (r < 0) die("read");
        if (r == 0) break;
        char* p = buf; ssize_t left = r;
        while (left > 0) {
            ssize_t w = send(ds, p, left, 0);
            if (w <= 0) die("send");
            left -= w; p += w; sent += w;
        }
    }
    close(fd); close(ds);
    printf("Enviado %s (%lld bytes)\n", base, sent);
    return 0;
}
