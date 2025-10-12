// clientMulti.c 
// Para ejecutar yo use:  ./clientMulti 192.168.1.212 36 texto.txt 49201 49200 49202

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFSZ 4096

static char *read_file(const char *path, int *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror("fopen"); return NULL; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(buf); return NULL; }
    fclose(f);
    buf[sz] = '\0';
    *out_len = (int)sz;
    return buf;
}

static int talk(const char *ip, int port, int target_port, int shift, const char *data, int len) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return -1; }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) { perror("inet_pton"); close(s); return -1; }

    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("connect"); close(s); return -1; }

    char header[128];
    int n = snprintf(header, sizeof(header), "%d %d %d\n", target_port, shift, len);
    send(s, header, n, 0);
    send(s, data, len, 0);

    // Lee primera línea: PROCESSED/REJECTED
    char line[BUFSZ]; int p=0;
    while (p+1 < (int)sizeof(line)) {
        char c; ssize_t r = recv(s, &c, 1, 0);
        if (r <= 0) break;
        line[p++] = c;
        if (c=='\n') break;
    }
    line[p] = '\0';

    printf("[client] %s:%d -> %s", ip, port, line);

    if (strncmp(line, "PROCESSED", 9) == 0) {
        ssize_t r;
        while ((r = recv(s, line, sizeof(line), 0)) > 0) {
            fwrite(line, 1, (size_t)r, stdout);
        }
        puts("");
    }

    close(s);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 7) {
        fprintf(stderr, "Uso: %s <IP> <SHIFT> <ARCHIVO> <TARGET_PORT> <PORT1> <PORT2> [PORT3 ...]\n", argv[0]);
        return 1;
    }
    const char *ip = argv[1];
    int shift = atoi(argv[2]);
    const char *path = argv[3];
    int target_port = atoi(argv[4]);

    int len=0; 
    char *data = read_file(path, &len);
    if (!data) { fprintf(stderr, "No pude leer %s\n", path); return 1; }

    for (int i=5; i<argc; i++) {
        int port = atoi(argv[i]);
        talk(ip, port, target_port, shift, data, len);
    }

    free(data);
    return 0;
}
