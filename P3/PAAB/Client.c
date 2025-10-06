#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUF 1024

static int read_line(int fd, char *buf, int max) {
    int i = 0;
    while (i < max - 1) {
        char c;
        int r = recv(fd, &c, 1, 0);
        if (r <= 0) return r;
        buf[i++] = c;
        if (c == '\n') break;
    }
    buf[i] = 0;
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

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Uso: %s <IP_SERVIDOR> <PUERTO_OBJETIVO> <SHIFT> <ARCHIVO>\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    int target = atoi(argv[2]);
    int shift  = atoi(argv[3]);
    const char *path = argv[4];

    // Leer archivo completo
    FILE *f = fopen(path, "rb");
    if (!f) { perror("fopen"); return 1; }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) { fprintf(stderr, "Archivo vacío\n"); fclose(f); return 1; }
    char *body = (char*)malloc(len);
    if (!body) { fprintf(stderr, "sin memoria\n"); fclose(f); return 1; }
    if (fread(body, 1, len, f) != (size_t)len) { perror("fread"); free(body); fclose(f); return 1; }
    fclose(f);

    int ports[3] = {49200, 49201, 49202};

    for (int i = 0; i < 3; i++) {
        int p = ports[i];

        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0) { perror("socket"); free(body); return 1; }

        struct sockaddr_in a;
        memset(&a, 0, sizeof(a));
        a.sin_family = AF_INET;
        a.sin_port = htons(p);
        if (inet_pton(AF_INET, server_ip, &a.sin_addr) != 1) { perror("inet_pton"); close(s); continue; }

        if (connect(s, (struct sockaddr*)&a, sizeof(a)) < 0) {
            perror("connect");
            printf("[CLIENTE] servidor %d: no conecta\n", p);
            close(s);
            continue;
        }

        // Enviar encabezado + cuerpo
        char hdr[256];
        int n = snprintf(hdr, sizeof(hdr), "TARGET_PORT:%d\nSHIFT:%d\nLEN:%ld\n\n", target, shift, len);
        send(s, hdr, n, 0);
        send(s, body, len, 0);

        // Leer respuesta
        char line[BUF];
        int r = read_line(s, line, sizeof line);
        if (r <= 0) {
            printf("[CLIENTE] servidor %d: sin respuesta\n", p);
            close(s);
            continue;
        }

        if (strncmp(line, "PROCESADO", 9) == 0) {
            int outlen = 0;
            if (read_line(s, line, sizeof line) > 0) sscanf(line, "LEN:%d", &outlen);
            read_line(s, line, sizeof line); // línea en blanco

            char *out = (char*)malloc(outlen + 1);
            if (!out) { fprintf(stderr, "sin memoria\n"); close(s); free(body); return 1; }
            if (read_n(s, out, outlen) != outlen) {
                printf("[CLIENTE] servidor %d: cuerpo incompleto\n", p);
                free(out); close(s); continue;
            }
            out[outlen] = 0;

            // Guardar en archivo sin mostrar texto
            char fname[64];
            snprintf(fname, sizeof fname, "out_%d.txt", p);
            FILE *fo = fopen(fname, "wb");
            if (fo) {
                fwrite(out, 1, outlen, fo);
                fclose(fo);
                printf("[CLIENTE] servidor %d → PROCESADO. Guardado en %s\n", p, fname);
            }

            free(out);
        } else {
            printf("[CLIENTE] servidor %d → RECHAZADO\n", p);
        }

        close(s);
    }

    free(body);
    return 0;
}
