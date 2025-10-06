#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

// Funciones auxiliares para leer/escribir exactamente n bytes
ssize_t readn(int fd, void *buf, size_t n) {
    size_t left = n;
    char *ptr = buf;
    while (left > 0) {
        ssize_t r = read(fd, ptr, left);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        } else if (r == 0) {
            break; // conexión cerrada
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

// Cifrado César
void caesar(char *s, size_t n, int shift) {
    int k = ((shift % 26) + 26) % 26; // normalizar
    for (size_t i = 0; i < n; i++) {
        if (s[i] >= 'A' && s[i] <= 'Z') {
            s[i] = 'A' + (s[i] - 'A' + k) % 26;
        } else if (s[i] >= 'a' && s[i] <= 'z') {
            s[i] = 'a' + (s[i] - 'a' + k) % 26;
        }
        // otros caracteres se dejan igual
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <PUERTO>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int listen_port = atoi(argv[1]);
    int servidor_fd, nuevo_socket;
    struct sockaddr_in direccion;

    // Crear socket
    servidor_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (servidor_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Configurar dirección
    direccion.sin_family = AF_INET;
    direccion.sin_addr.s_addr = INADDR_ANY;
    direccion.sin_port = htons(listen_port);

    // Enlazar socket
    if (bind(servidor_fd, (struct sockaddr *)&direccion, sizeof(direccion)) < 0) {
        perror("bind");
        close(servidor_fd);
        exit(EXIT_FAILURE);
    }

    // Escuchar conexiones
    if (listen(servidor_fd, 3) < 0) {
        perror("listen");
        close(servidor_fd);
        exit(EXIT_FAILURE);
    }

    printf("[SERVER %d] Escuchando...\n", listen_port);

    // Aceptar conexiones en bucle
    while (1) {
        nuevo_socket = accept(servidor_fd, NULL, NULL);
        if (nuevo_socket < 0) {
            perror("accept");
            continue;
        }

        // Leer cabecera: target_port, shift, text_len
        uint32_t net_target_port, net_shift, net_len;
        if (readn(nuevo_socket, &net_target_port, 4) != 4 ||
            readn(nuevo_socket, &net_shift, 4) != 4 ||
            readn(nuevo_socket, &net_len, 4) != 4) {
            perror("read header");
            close(nuevo_socket);
            continue;
        }

        uint32_t target_port = ntohl(net_target_port);
        uint32_t shift = ntohl(net_shift);
        uint32_t text_len = ntohl(net_len);

        char *buffer = NULL;
        if (text_len > 0) {
            buffer = malloc(text_len);
            if (!buffer) {
                perror("malloc");
                close(nuevo_socket);
                continue;
            }
            if (readn(nuevo_socket, buffer, text_len) != (ssize_t)text_len) {
                perror("read payload");
                free(buffer);
                close(nuevo_socket);
                continue;
            }
        }

        // Decisión
        int procesar = (target_port == (uint32_t)listen_port);
        uint32_t status = procesar ? 1 : 0;
        uint32_t resp_len = procesar ? text_len : 0;

        if (procesar && buffer) {
            caesar(buffer, text_len, shift);
            printf("[SERVER %d] PROCESADO (shift=%u, len=%u)\n",
                   listen_port, shift, text_len);
        } else {
            printf("[SERVER %d] RECHAZADO (target=%u)\n",
                   listen_port, target_port);
        }

        // Enviar respuesta
        uint32_t net_status = htonl(status);
        uint32_t net_resp_len = htonl(resp_len);
        writen(nuevo_socket, &net_status, 4);
        writen(nuevo_socket, &net_resp_len, 4);
        if (procesar && buffer) {
            writen(nuevo_socket, buffer, resp_len);
        }

        free(buffer);
        close(nuevo_socket);
    }

    close(servidor_fd);
    return 0;
}

