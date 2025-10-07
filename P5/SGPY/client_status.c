// client_status.c
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static void die(const char* m) { perror(m); exit(1); }

static int connect_host_port(const char* host, int port) {
    struct addrinfo hints = { 0 }, * res = 0;
    hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    char portstr[16]; snprintf(portstr, sizeof(portstr), "%d", port);
    int e = getaddrinfo(host, portstr, &hints, &res);
    if (e) { fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(e)); exit(1); }
    int s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s < 0) die("socket");
    if (connect(s, res->ai_addr, res->ai_addrlen) < 0) die("connect");
    freeaddrinfo(res);
    return s;
}
static ssize_t read_line(int s, char* buf, size_t n) {
    size_t i = 0; char c;
    while (i + 1 < n) {
        ssize_t r = recv(s, &c, 1, 0);
        if (r <= 0) return r;
        if (c == '\n') { buf[i] = '\0'; return (ssize_t)i; }
        buf[i++] = c;
    } buf[i] = '\0'; return (ssize_t)i;
}
int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "Uso: %s <server_host> <control_port> <logfile>\n", argv[0]); return 1; }
    const char* host = argv[1]; int port = atoi(argv[2]); const char* logf = argv[3];
    int s = connect_host_port(host, port);
    const char* cmd = "STATUS\n"; send(s, cmd, strlen(cmd), 0);
    char line[128] = { 0 }; if (read_line(s, line, sizeof(line)) <= 0) { close(s); return 1; }
    close(s);

    time_t t = time(NULL); struct tm tm; localtime_r(&t, &tm);
    char ts[64]; strftime(ts, sizeof(ts), "%F %T", &tm);

    FILE* f = fopen(logf, "a");
    if (!f) { perror("fopen log"); return 1; }
    fprintf(f, "%s %s\n", ts, line);
    fclose(f);
    printf("%s %s\n", ts, line);
    return 0;
}
