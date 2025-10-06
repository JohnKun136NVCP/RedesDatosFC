#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUF 8192

int send_all(int sock, const char *buf, int len) {
    int sent = 0;
    while (sent < len) {
        int n = send(sock, buf + sent, len - sent, 0);
        if (n <= 0) return -1;
        sent += n;
    }
    return 0;
}

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

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Uso: %s <IP> <PUERTO_OBJETIVO> <SHIFT> <archivo>\n", argv[0]);
        printf("Ej.: %s 192.168.100.200 49201 10 archivo.txt\n", argv[0]);
        return 1;
    }

    const char *ip = argv[1];
    int target_port = atoi(argv[2]);
    int shift = atoi(argv[3]);
    const char *fname = argv[4];

    // Leer archivo completo a memoria
    FILE *f = fopen(fname, "rb");
    if (!f) { perror("archivo"); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *data = NULL;
    if (size > 0) {
        data = (char*)malloc((size_t)size);
        if (!data) { fclose(f); fprintf(stderr, "sin memoria\n"); return 1; }
        if (fread(data, 1, (size_t)size, f) != (size_t)size) {
            fclose(f); free(data); fprintf(stderr, "error leyendo\n"); return 1;
        }
    }
    fclose(f);

    int ports[3] = {49200, 49201, 49202};

    for (int i = 0; i < 3; i++) {
        int p = ports[i];

        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0) { perror("socket"); continue; }

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port   = htons((unsigned short)p);
        addr.sin_addr.s_addr = inet_addr(ip);

        if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("connect");
            close(s);
            continue;
        }

        // Enviar encabezado + datos
        char header[128];
        int n = snprintf(header, sizeof(header),
                         "PORT %d SHIFT %d SIZE %ld\n", target_port, shift, size);
        if (send_all(s, header, n) < 0) { close(s); continue; }
        if (size > 0 && send_all(s, data, (int)size) < 0) { close(s); continue; }

        // Leer respuesta
        char line[64];
        if (recv_line(s, line, sizeof(line)) <= 0) { close(s); continue; }

        if (strncmp(line, "PROCESSED", 9) == 0) {
            // Guardar todo lo que llegue después como resultado
            char outname[64];
            snprintf(outname, sizeof(outname), "resultado_%d.txt", p);
            FILE *out = fopen(outname, "wb");
            if (!out) { perror("fopen"); close(s); continue; }

            char buf[BUF];
            int r;
            while ((r = recv(s, buf, sizeof(buf), 0)) > 0) {
                fwrite(buf, 1, r, out);
            }
            fclose(out);
            printf("[CLIENTE] Puerto %d -> PROCESSED (guardado en %s)\n", p, outname);
        } else {
            printf("[CLIENTE] Puerto %d -> REJECT\n", p);
        }

        close(s);
    }

    if (data) free(data);
    return 0;
}

