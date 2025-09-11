#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

// Funciones auxiliares
ssize_t readn(int fd, void *buf, size_t n) {
    size_t left = n;
    char *ptr = buf;
    while (left > 0) {
        ssize_t r = read(fd, ptr, left);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        } else if (r == 0) {
            break;
        }
        left -= r;
        ptr += r;
    }
    return n - left;
}

ssize_t writen(int fd, const void *buf, size_t n) {
    size_t left = n;
    const char *ptr = buf;
    while (left > 0) {
        ssize_t w = write(fd, ptr, left);
        if (w <= 0) {
            if (w < 0 && errno == EINTR) continue;
            return -1;
        }
        left -= w;
        ptr += w;
    }
    return n;
}

char *leerArchivo(const char *path, size_t *len_out) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror("fopen"); return NULL; }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);

    char *buf = malloc(sz);
    if (!buf) { perror("malloc"); fclose(f); return NULL; }

    size_t leidos = fread(buf, 1, sz, f);
    fclose(f);

    if (leidos != (size_t)sz) {
        fprintf(stderr, "Error al leer archivo\n");
        free(buf);
        return NULL;
    }

    *len_out = sz;
    return buf;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Uso: %s <IP_SERVIDOR> <PUERTO_OBJETIVO> <DESPLAZAMIENTO> <ARCHIVO>\n", argv[0]);
        return 1;
    }

    const char *ip = argv[1];
    int target_port = atoi(argv[2]);
    int shift = atoi(argv[3]);
    const char *archivo = argv[4];

    size_t text_len;
    char *texto = leerArchivo(archivo, &text_len);
    if (!texto) return 1;

    int puertos[3] = {49200, 49201, 49202};

    for (int i = 0; i < 3; i++) {
        int cliente_fd;
        struct sockaddr_in direccion;

        // Crear socket
        cliente_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (cliente_fd < 0) { perror("socket"); continue; }

        // Configurar dirección
        direccion.sin_family = AF_INET;
        direccion.sin_port = htons(puertos[i]);
        inet_pton(AF_INET, ip, &direccion.sin_addr);

        // Conectar al servidor
        if (connect(cliente_fd, (struct sockaddr *)&direccion, sizeof(direccion)) < 0) {
            perror("connect");
            close(cliente_fd);
            continue;
        }

        // Armar mensaje (target_port, shift, text_len, texto)
        uint32_t net_target_port = htonl(target_port);
        uint32_t net_shift = htonl(shift);
        uint32_t net_len = htonl(text_len);

        writen(cliente_fd, &net_target_port, 4);
        writen(cliente_fd, &net_shift, 4);
        writen(cliente_fd, &net_len, 4);
        if (text_len > 0) writen(cliente_fd, texto, text_len);

        // Recibir respuesta
        uint32_t net_status, net_resp_len;
        if (readn(cliente_fd, &net_status, 4) != 4 ||
            readn(cliente_fd, &net_resp_len, 4) != 4) {
            perror("read");
            close(cliente_fd);
            continue;
        }

        uint32_t status = ntohl(net_status);
        uint32_t resp_len = ntohl(net_resp_len);

        if (status == 1) {
            char *resp = malloc(resp_len + 1);
            if (resp_len > 0) {
                readn(cliente_fd, resp, resp_len);
            }
            resp[resp_len] = '\0';
            printf("[CLIENTE] Servidor %d: PROCESADO\n", puertos[i]);
            printf("Texto cifrado: %s\n", resp);
            free(resp);
        } else {
            printf("[CLIENTE] Servidor %d: RECHAZADO\n", puertos[i]);
        }

        close(cliente_fd);
    }

    free(texto);
    return 0;
}

