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
    if (argc != 9) {
        printf("Uso: %s <IP_SERVIDOR> <PUERTO1> <PUERTO2> <PUERTO3> <ARCH1> <ARCH2> <ARCH3> <SHIFT>\n", argv[0]);
        return 1;
    }

    const char *ip = argv[1];
    int ports[3] = { atoi(argv[2]), atoi(argv[3]), atoi(argv[4]) };
    const char *files[3] = { argv[5], argv[6], argv[7] };
    int shift = atoi(argv[8]);

    for (int i = 0; i < 3; i++) {
        // Leer archivo 
        FILE *f = fopen(files[i], "rb");
        if (!f) { perror("fopen"); continue; }
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (len <= 0) { fprintf(stderr, "[CLIENTE] %d:%s archivo vacío\n", ports[i], files[i]); fclose(f); continue; }
        char *body = (char*)malloc(len);
        if (!body) { fprintf(stderr, "sin memoria\n"); fclose(f); continue; }
        if (fread(body, 1, len, f) != (size_t)len) { perror("fread"); free(body); fclose(f); continue; }
        fclose(f);

        // Conectar a puerto i
        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0) { perror("socket"); free(body); continue; }
        struct sockaddr_in a; memset(&a, 0, sizeof a);
        a.sin_family = AF_INET; a.sin_port = htons(ports[i]);
        if (inet_pton(AF_INET, ip, &a.sin_addr) != 1) { perror("inet_pton"); close(s); free(body); continue; }
        if (connect(s, (struct sockaddr*)&a, sizeof a) < 0) {
            perror("connect");
            printf("[CLIENTE] servidor %d: no conecta\n", ports[i]);
            close(s); free(body); continue;
        }

        // Enviar encabezado + cuerpo 
        char hdr[256];
        int n = snprintf(hdr, sizeof hdr, "TARGET_PORT:%d\nSHIFT:%d\nLEN:%ld\n\n",
                         ports[i], shift, len);
        send(s, hdr, n, 0);
        send(s, body, len, 0);

        // Leer respuesta
        char line[BUF];
        int r = read_line(s, line, sizeof line);
        if (r <= 0) {
            printf("[CLIENTE] servidor %d: sin respuesta\n", ports[i]);
            close(s); free(body); continue;
        }

        if (strncmp(line, "PROCESADO", 9) == 0) {
            int outlen = 0;
            if (read_line(s, line, sizeof line) > 0) sscanf(line, "LEN:%d", &outlen);
            read_line(s, line, sizeof line); /* blanco */

            char *out = (char*)malloc(outlen + 1);
            if (!out) { fprintf(stderr, "sin memoria\n"); close(s); free(body); return 1; }
            if (read_n(s, out, outlen) != outlen) {
                printf("[CLIENTE] servidor %d: cuerpo incompleto\n", ports[i]);
                free(out); close(s); free(body); continue;
            }
            out[outlen] = 0;

            // Guardar
            char fname[128];
            const char *slash = strrchr(files[i], '/');
            const char *base = slash ? slash + 1 : files[i];
            snprintf(fname, sizeof fname, "out_%d_%s.txt", ports[i], base);
            FILE *fo = fopen(fname, "wb");
            if (fo) { fwrite(out, 1, outlen, fo); fclose(fo); }
            printf("[CLIENTE] %d → PROCESADO. Guardado en %s\n", ports[i], fname);

            free(out);
        } else {
            printf("[CLIENTE] %d → RECHAZADO\n", ports[i]);
        }

        close(s);
        free(body);
    }

    return 0;
}
