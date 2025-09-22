#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/stat.h>

#define PORT 7006
#define BUFFER_SIZE 1024

static int read_line(int fd, char *buf, int max) {
    int i = 0;
    while (i < max - 1) {
        char c; int r = recv(fd, &c, 1, 0);
        if (r <= 0) return r;
        buf[i++] = c;
        if (c == '\n') break;
    }
    buf[i] = '\0';
    return i;
}
static int read_n(int fd, char *buf, int n) {
    int got = 0;
    while (got < n) {
        int r = recv(fd, buf + got, n - got, 0);
        if (r <= 0) return r;
        got += r;
    }
    return got;
}

// abre un socket de escucha en el primer puerto libre en [base+1, base+200]
static int open_worker(int base, int *out_port) {
    for (int p = base + 1; p <= base + 200; p++) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) return -1;
        int opt = 1; setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
        struct sockaddr_in a; memset(&a, 0, sizeof a);
        a.sin_family = AF_INET; a.sin_addr.s_addr = INADDR_ANY; a.sin_port = htons(p);
        if (bind(fd, (struct sockaddr*)&a, sizeof a) == 0 && listen(fd, 8) == 0) {
            *out_port = p; return fd;
        }
        close(fd);
    }
    return -1;
}

// decide carpeta segun IP local del socket aceptado (compara con s01/s02)
static const char* pick_dir(int sock) {
    static char path[256];
    struct sockaddr_in local; socklen_t slen = sizeof local;
    if (getsockname(sock, (struct sockaddr*)&local, &slen) == 0) {
        struct hostent *h1 = gethostbyname("s01");
        struct hostent *h2 = gethostbyname("s02");
        in_addr_t ip1 = (h1 && h1->h_addr_list && h1->h_addr_list[0]) ? *(in_addr_t*)h1->h_addr_list[0] : 0;
        in_addr_t ip2 = (h2 && h2->h_addr_list && h2->h_addr_list[0]) ? *(in_addr_t*)h2->h_addr_list[0] : 0;

        if (ip1 && local.sin_addr.s_addr == ip1) {
            snprintf(path, sizeof path, "%s/s01", getenv("HOME")); return path;
        }
        if (ip2 && local.sin_addr.s_addr == ip2) {
            snprintf(path, sizeof path, "%s/s02", getenv("HOME")); return path;
        }
        // Fallback por último octeto (101/102)
        unsigned last = (unsigned)(ntohl(local.sin_addr.s_addr) & 0xFF);
        if (last == 101) { snprintf(path, sizeof path, "%s/s01", getenv("HOME")); return path; }
        if (last == 102) { snprintf(path, sizeof path, "%s/s02", getenv("HOME")); return path; }
    }
    snprintf(path, sizeof path, "%s/s01", getenv("HOME"));
    return path;
}

int main(int argc, char **argv) {
    int base_port = (argc == 2) ? atoi(argv[1]) : PORT;

    int sbase = socket(AF_INET, SOCK_STREAM, 0);
    if (sbase == -1) { perror("socket"); return 1; }
    int opt = 1; setsockopt(sbase, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);

    struct sockaddr_in addr; memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET; addr.sin_port = htons(base_port); addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sbase, (struct sockaddr*)&addr, sizeof addr) < 0) { perror("bind"); close(sbase); return 1; }
    if (listen(sbase, 8) < 0) { perror("listen"); close(sbase); return 1; }

    printf("[*][SERVER %d] LISTENING (base)...\n", base_port);

    // asegurar carpetas
    char p1[256], p2[256];
    snprintf(p1, sizeof p1, "%s/s01", getenv("HOME"));
    snprintf(p2, sizeof p2, "%s/s02", getenv("HOME"));
    mkdir(p1, 0755); mkdir(p2, 0755);

    while (1) {
        struct sockaddr_in caddr; socklen_t alen = sizeof caddr;
        int c0 = accept(sbase, (struct sockaddr*)&caddr, &alen);
        if (c0 < 0) { perror("accept"); continue; }

        int wport = 0;
        int wlisten = open_worker(base_port, &wport);
        if (wlisten < 0) {
            const char *err = "PORT:0\n\n";
            send(c0, err, strlen(err), 0);
            close(c0);
            continue;
        }
        char line[64];
        int n = snprintf(line, sizeof line, "PORT:%d\n\n", wport);
        send(c0, line, n, 0);
        close(c0);

        printf("[*] asignado puerto %d, esperando reconexión...\n", wport);

        struct sockaddr_in cc; socklen_t clen = sizeof cc;
        int cs = accept(wlisten, (struct sockaddr*)&cc, &clen);
        if (cs < 0) { perror("accept worker"); close(wlisten); continue; }

        int len = 0; char name[BUFFER_SIZE] = {0};
        if (read_line(cs, line, sizeof line) <= 0) { close(cs); close(wlisten); continue; }
        sscanf(line, "LEN:%d", &len);
        if (read_line(cs, line, sizeof line) <= 0) { close(cs); close(wlisten); continue; }
        if (!strncmp(line, "NAME:", 5)) {
            strncpy(name, line + 5, sizeof(name) - 1);
            size_t L = strlen(name); if (L && name[L-1] == '\n') name[L-1] = 0;
        }
        read_line(cs, line, sizeof line); 

        if (len <= 0 || len > (1<<22)) { const char *rej="ERR\n"; send(cs, rej, strlen(rej), 0); close(cs); close(wlisten); continue; }

        char *body = (char*)malloc(len);
        if (!body) { const char *rej="ERR\n"; send(cs, rej, strlen(rej), 0); close(cs); close(wlisten); continue; }
        if (read_n(cs, body, len) != len) { free(body); close(cs); close(wlisten); continue; }

        const char *dir = pick_dir(cs);
        char outpath[512];
        snprintf(outpath, sizeof outpath, "%s/%s", dir, name[0] ? name : "recv.txt");

        FILE *fo = fopen(outpath, "wb");
        if (!fo) { const char *rej="ERR\n"; send(cs, rej, strlen(rej), 0); free(body); close(cs); close(wlisten); continue; }
        fwrite(body, 1, len, fo); fclose(fo); free(body);

        const char *ok = "OK\n"; send(cs, ok, strlen(ok), 0);
        printf("[*] guardado: %s\n", outpath);

        close(cs);
        close(wlisten);
    }

    close(sbase);
    return 0;
}