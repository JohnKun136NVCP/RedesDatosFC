#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUF 8192

// Enviar todo el buffer (simple)
int send_all(int sock, const char *buf, int len) {
    int sent = 0;
    while (sent < len) {
        int n = send(sock, buf + sent, len - sent, 0);
        if (n <= 0) return -1;
        sent += n;
    }
    return 0;
}

// Recibir una línea terminada en '\n' (simple)
int recv_line(int sock, char *line, int max) {
    int i = 0;
    while (i < max - 1) {
        char c;
        int n = recv(sock, &c, 1, 0);
        if (n <= 0) return -1;
        line[i++] = c;
        if (c == '\n') break;
    }
    line[i] = '\0';
    return i;
}

// Leer archivo completo en memoria (simple)
int leer_archivo(const char *fname, char **out_data, long *out_size) {
    FILE *f = fopen(fname, "rb");
    if (!f) { perror("fopen"); return -1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *data = NULL;
    if (size > 0) {
        data = (char*)malloc((size_t)size);
        if (!data) { fclose(f); fprintf(stderr, "sin memoria\n"); return -1; }
        if (fread(data, 1, (size_t)size, f) != (size_t)size) {
            fclose(f); free(data); fprintf(stderr, "error leyendo archivo\n"); return -1;
        }
    }
    fclose(f);
    *out_data = data;
    *out_size = size;
    return 0;
}

int main(int argc, char *argv[]) {
    // ./clientMulti <IP> <PUERTO1> <PUERTO2> <PUERTO3> <file1> <file2> <file3> <SHIFT>
    if (argc != 9) {
        printf("Uso: %s <IP> <PUERTO1> <PUERTO2> <PUERTO3> <file1> <file2> <file3> <SHIFT>\n", argv[0]);
        printf("Ej.: %s 192.168.100.200 49200 49201 49202 file1.txt file2.txt file3.txt 34\n", argv[0]);
        return 1;
    }

    const char *ip = argv[1];

    int ports[3];
    ports[0] = atoi(argv[2]);
    ports[1] = atoi(argv[3]);
    ports[2] = atoi(argv[4]);

    const char *files[3];
    files[0] = argv[5];
    files[1] = argv[6];
    files[2] = argv[7];

    int shift = atoi(argv[8]);

    for (int i = 0; i < 3; i++) {
        int p = ports[i];
        const char *fname = files[i];

        // Cargar archivo a memoria
        char *data = NULL;
        long size = 0;
        if (leer_archivo(fname, &data, &size) < 0) {
            fprintf(stderr, "[CLIENTE] Puerto %d: no pude leer %s\n", p, fname);
            continue;
        }

        // Conectar
        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0) { perror("socket"); if (data) free(data); continue; }

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port   = htons((unsigned short)p);
        addr.sin_addr.s_addr = inet_addr(ip);

        if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("connect");
            close(s);
            if (data) free(data);
            continue;
        }

        // Encabezado: el "PORT" es el puerto objetivo que debe procesar
        char header[128];
        int n = snprintf(header, sizeof(header), "PORT %d SHIFT %d SIZE %ld\n", p, shift, size);
        if (send_all(s, header, n) < 0) { close(s); if (data) free(data); continue; }

        if (size > 0) {
            if (send_all(s, data, (int)size) < 0) { close(s); if (data) free(data); continue; }
        }

        // Leer respuesta inicial del servidor
        char line[64];
        if (recv_line(s, line, sizeof(line)) <= 0) {
            printf("[CLIENTE] Puerto %d: sin respuesta\n", p);
            close(s);
            if (data) free(data);
            continue;
        }

        if (strncmp(line, "PROCESSED", 9) == 0) {
            // Guardar lo que siga (texto cifrado)
            char outname[64];
            snprintf(outname, sizeof(outname), "file_%d_cesar.txt", p);
            FILE *out = fopen(outname, "wb");
            if (!out) { perror("fopen"); close(s); if (data) free(data); continue; }

            char buf[BUF];
            int r;
            while ((r = recv(s, buf, sizeof(buf), 0)) > 0) {
                fwrite(buf, 1, r, out);
            }
            fclose(out);
            printf("[CLIENTE] Puerto %d: ARCHIVO CIFRADO RECIBIDO (guardado en %s)\n", p, outname);
        } else {
            printf("[CLIENTE] Puerto %d: RECHAZADO\n", p);
        }

        close(s);
        if (data) free(data);
    }

    return 0;
}

