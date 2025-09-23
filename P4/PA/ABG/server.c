// server.c
// Uso: ./server <PUERTO>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BACKLOG 8
#define BUFSZ   4096

static void caesar(char *s, int shift) {
    shift = ((shift % 26) + 26) % 26;
    for (size_t i = 0; s[i]; i++) {
        unsigned char c = (unsigned char)s[i];
        if (isupper(c)) s[i] = (char)(((c - 'A' + shift) % 26) + 'A');
        else if (islower(c)) s[i] = (char)(((c - 'a' + shift) % 26) + 'a');
    }
}

static int read_line(int fd, char *out, size_t cap) {
    size_t n = 0;
    while (n + 1 < cap) {
        char c;
        ssize_t r = recv(fd, &c, 1, 0);
        if (r <= 0) return -1;
        out[n++] = c;
        if (c == '\n') break;
    }
    out[n] = '\0';
    return (int)n;
}

static void nowstamp(char *dst, size_t n) {
    time_t t = time(NULL);
    struct tm tm; localtime_r(&t, &tm);
    strftime(dst, n, "%Y%m%d-%H%M%S", &tm);
}

static void ensure_inbox(void) {
    struct stat st;
    if (stat("inbox", &st) == -1) mkdir("inbox", 0755);
}

int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "Uso: %s <PUERTO>\n", argv[0]); return 1; }
    int my_port = atoi(argv[1]);

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return 1; }
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &yes, sizeof(yes));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)my_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(s, BACKLOG) < 0) { perror("listen"); return 1; }

    printf("[server %d] escuchando...\n", my_port);
    ensure_inbox();

    for (;;) {
        int c = accept(s, NULL, NULL);
        if (c < 0) { perror("accept"); continue; }

        char ts[32]; nowstamp(ts, sizeof(ts));
        char header[512] = {0};
        int hl = read_line(c, header, sizeof(header));

        int target = 0, shift = 0, len = -1;
        char fname[256] = {0};
        int parsed = 0;

        if (hl > 0) {
            // Formato con filename opcional: TARGET SHIFT LEN [FILENAME]
            int n = sscanf(header, "%d %d %d %255s", &target, &shift, &len, fname);
            if (n >= 3 && len >= 0 && len <= 100000000) parsed = 1;
        }

        if (parsed && target == my_port) {
            // --- MODO 1: protocolo con header ---
            size_t need = (size_t)len;
            char *payload = (char*)malloc(need + 1);
            if (!payload) { perror("malloc"); close(c); continue; }

            size_t got = 0;
            while (got < need) {
                ssize_t r = recv(c, payload + got, need - got, 0);
                if (r <= 0) break;
                got += (size_t)r;
            }
            payload[got] = '\0';

            if (got == need) {
                // nombres de salida
                char rawpath[512], procpath[512];
                if (fname[0]) {
                    snprintf(rawpath,  sizeof(rawpath), "inbox/raw-%s-%d-%s",  ts, my_port, fname);
                    snprintf(procpath, sizeof(procpath), "inbox/proc-%s-%d-%s", ts, my_port, fname);
                } else {
                    snprintf(rawpath,  sizeof(rawpath), "inbox/raw-%s-%d.bin",  ts, my_port);
                    snprintf(procpath, sizeof(procpath), "inbox/proc-%s-%d.bin", ts, my_port);
                }

                FILE *fr = fopen(rawpath, "wb");
                if (fr) { fwrite(payload, 1, got, fr); fclose(fr); }

                caesar(payload, shift);

                FILE *fp = fopen(procpath, "wb");
                if (fp) { fwrite(payload, 1, got, fp); fclose(fp); }

                const char *ok = "PROCESSED\n";
                send(c, ok, strlen(ok), 0);
                send(c, payload, got, 0);

                printf("[%s] puerto=%d OK len=%zu raw=%s proc=%s\n",
                       ts, my_port, got, rawpath, procpath);
            } else {
                const char *rej = "REJECTED\n";
                send(c, rej, strlen(rej), 0);
                printf("[%s] puerto=%d REJECTED (incompleto)\n", ts, my_port);
            }
            free(payload);
            close(c);
            continue;
        }

        // --- MODO 2: sin header ---
        // guarda todo lo que llegue en un archivo único y responde RECEIVED
        char fbpath[512];
        snprintf(fbpath, sizeof(fbpath), "inbox/fallback-%s-%d.bin", ts, my_port);

        FILE *ff = fopen(fbpath, "wb");
        if (!ff) { perror("fopen"); close(c); continue; }

        // el header leído (si lo hubo) también lo guardamos como primera porción
        if (hl > 0) fwrite(header, 1, (size_t)hl, ff);

        char buf[BUFSZ];
        ssize_t r;
        size_t total = (hl > 0 ? (size_t)hl : 0);
        while ((r = recv(c, buf, sizeof(buf), 0)) > 0) {
            fwrite(buf, 1, (size_t)r, ff);
            total += (size_t)r;
        }
        fclose(ff);

        const char *rcv = "RECEIVED\n";
        send(c, rcv, strlen(rcv), 0);
        printf("[%s] puerto=%d FALLBACK len=%zu file=%s\n", ts, my_port, total, fbpath);

        close(c);
    }
}
