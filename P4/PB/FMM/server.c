// serverPB.c
// Uso: ./serverPB <PUERTO>

#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BACKLOG   64
#define BUFSZ     4096
#define MAX_LSN   16

static const char *home_dir(void) {
    const char *h = getenv("HOME");
    if (h && *h) return h;
    return ".";
}

static void stamp_now(char *dst, size_t cap) {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(dst, cap, "%Y%m%d_%H%M%S", &tm);
}

// resuelve a hostname/alias a IPv4 string (es decir "s01" -> "192.168.x.y")
static int resolve_ipv4(const char *name, char out_ip[INET_ADDRSTRLEN]) {
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    int rc = getaddrinfo(name, NULL, &hints, &res);
    if (rc != 0 || res == NULL) return -1;

    struct sockaddr_in *sin = (struct sockaddr_in *)res->ai_addr;
    if (inet_ntop(AF_INET, &sin->sin_addr, out_ip, INET_ADDRSTRLEN) == NULL) {
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);
    return 0;
}

// escucha un específico IPV4 y puerto
static int listen_on_ip(const char *ip, int port) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return -1; }

    int yes = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        perror("setsockopt");
        close(s);
        return -1;
    }

    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port   = htons((uint16_t)port);
    if (inet_pton(AF_INET, ip, &a.sin_addr) != 1) {
        fprintf(stderr, "inet_pton failed for '%s'\n", ip);
        close(s);
        return -1;
    }

    if (bind(s, (struct sockaddr *)&a, sizeof(a)) < 0) {
        perror("bind");
        close(s);
        return -1;
    }

    if (listen(s, BACKLOG) < 0) {
        perror("listen");
        close(s);
        return -1;
    }

    return s;
}

struct Listener {
    int  fd;
    int  port;
    char alias[16];
    char ip[INET_ADDRSTRLEN];
};

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <PUERTO> [s01 s02 s03 s04]\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);

    struct Listener ls[MAX_LSN];
    int nls = 0;

    if (argc == 2) {
        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0) { perror("socket"); return 1; }

        int yes = 1;
        if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
            perror("setsockopt");
            close(s);
            return 1;
        }

        struct sockaddr_in a;
        memset(&a, 0, sizeof(a));
        a.sin_family      = AF_INET;
        a.sin_port        = htons((uint16_t)port);
        a.sin_addr.s_addr = INADDR_ANY;

        if (bind(s, (struct sockaddr *)&a, sizeof(a)) < 0) {
            perror("bind");
            close(s);
            return 1;
        }

        if (listen(s, BACKLOG) < 0) {
            perror("listen");
            close(s);
            return 1;
        }

        ls[nls].fd   = s;
        ls[nls].port = port;
        snprintf(ls[nls].alias, sizeof(ls[nls].alias), "%s", "recv");
        snprintf(ls[nls].ip,    sizeof(ls[nls].ip),    "%s", "0.0.0.0");
        nls++;

        printf("[SERVER] Listening on 0.0.0.0:%d\n", port);
    } else {
        for (int i = 2; i < argc && nls < MAX_LSN; i++) {
            char ip[INET_ADDRSTRLEN];
            if (resolve_ipv4(argv[i], ip) != 0) {
                fprintf(stderr, "[SERVER] WARN: cannot resolve %s\n", argv[i]);
                continue;
            }

            int fd = listen_on_ip(ip, port);
            if (fd < 0) {
                continue;
            }

            ls[nls].fd   = fd;
            ls[nls].port = port;
            snprintf(ls[nls].alias, sizeof(ls[nls].alias), "%s", argv[i]);
            snprintf(ls[nls].ip,    sizeof(ls[nls].ip),    "%s", ip);
            printf("[SERVER] Listening on %s (%s):%d\n", ls[nls].alias, ls[nls].ip, port);
            nls++;
        }

        if (nls == 0) {
            fprintf(stderr, "[SERVER] ERROR: no listeners active\n");
            return 1;
        }
    }

    for (;;) {
        fd_set rfds;
        FD_ZERO(&rfds);

        int mx = -1;
        for (int i = 0; i < nls; i++) {
            FD_SET(ls[i].fd, &rfds);
            if (ls[i].fd > mx) mx = ls[i].fd;
        }

        int rc = select(mx + 1, &rfds, NULL, NULL, NULL);
        if (rc < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        for (int i = 0; i < nls; i++) {
            if (!FD_ISSET(ls[i].fd, &rfds)) continue;

            struct sockaddr_in cli;
            socklen_t cl = sizeof(cli);
            int cfd = accept(ls[i].fd, (struct sockaddr *)&cli, &cl);
            if (cfd < 0) {
                if (errno == EINTR) continue;
                perror("accept");
                continue;
            }

            const char *home = home_dir();

            char ts[32];
            stamp_now(ts, sizeof(ts));

            char outdir[512];
            snprintf(outdir, sizeof(outdir), "%s/%s", home, ls[i].alias);

            char path[1024];
            snprintf(path, sizeof(path), "%s/recv_%s.txt", outdir, ts);

            FILE *f = fopen(path, "wb");
            if (f == NULL) {
                perror("fopen");
                close(cfd);
                continue;
            }

            printf("[SERVER] Conexión aceptada en %s\n", ls[i].alias);
            fflush(stdout);

            char buf[BUFSZ];
            size_t total = 0;

            for (;;) {
                ssize_t r = recv(cfd, buf, sizeof(buf), 0);
                if (r == 0) break;
                if (r < 0) {
                    if (errno == EINTR) continue;
                    perror("recv");
                    break;
                }

                size_t w = fwrite(buf, 1, (size_t)r, f);
                if (w != (size_t)r) {
                    perror("fwrite");
                    break;
                }
                total += (size_t)r;
            }

            fclose(f);
            close(cfd);

            printf("[SERVER] Archivo guardado en: %s (%zu bytes)\n", path, total);
            fflush(stdout);
        }
    }

    for (int i = 0; i < nls; i++) close(ls[i].fd);
    return 0;
}