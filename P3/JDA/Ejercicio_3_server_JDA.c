#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

// Las funciones auxiliares readn, writen y caesar permanecen exactamente iguales
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

void caesar(char *s, size_t n, int shift) {
    int k = ((shift % 26) + 26) % 26; // normalizar
    for (size_t i = 0; i < n; i++) {
        if (s[i] >= 'A' && s[i] <= 'Z') {
            s[i] = 'A' + (s[i] - 'A' + k) % 26;
        } else if (s[i] >= 'a' && s[i] <= 'z') {
            s[i] = 'a' + (s[i] - 'a' + k) % 26;
        }
    }
}

// Función que contiene la lógica del servidor original para un puerto
void iniciar_servidor(int listen_port) {
    int servidor_fd, nuevo_socket;
    struct sockaddr_in direccion;

    servidor_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (servidor_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    direccion.sin_family = AF_INET;
    direccion.sin_addr.s_addr = INADDR_ANY;
    direccion.sin_port = htons(listen_port);

    if (bind(servidor_fd, (struct sockaddr *)&direccion, sizeof(direccion)) < 0) {
        perror("bind");
        close(servidor_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(servidor_fd, 5) < 0) {
        perror("listen");
        close(servidor_fd);
        exit(EXIT_FAILURE);
    }

    printf("[Servidor Padre PID: %d] Escuchando en el puerto %d\n", getppid(), listen_port);

    while (1) {
        nuevo_socket = accept(servidor_fd, NULL, NULL);
        if (nuevo_socket < 0) {
            perror("accept");
            continue;
        }

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

        int procesar = (target_port == (uint32_t)listen_port);
        uint32_t status = procesar ? 1 : 0;
        uint32_t resp_len = procesar ? text_len : 0;

        if (procesar && buffer) {
            caesar(buffer, text_len, shift);
            printf("[SERVER %d] PROCESADO (shift=%u, len=%u)\n", listen_port, shift, text_len);
        } else {
            printf("[SERVER %d] RECHAZADO (target=%u)\n", listen_port, target_port);
        }

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
}

int main(void) { // No necesita argumentos
    int puertos[] = {49200, 49201, 49202};
    int num_puertos = sizeof(puertos) / sizeof(puertos[0]);

    printf("Iniciando servidor maestro (PID: %d)\n", getpid());

    for (int i = 0; i < num_puertos; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) {
            // Este es el proceso HIJO
            // Inicia la lógica del servidor para un puerto y nunca retorna
            iniciar_servidor(puertos[i]);
            exit(0); // El hijo termina aquí
        }
        // El proceso PADRE continúa el bucle para crear el siguiente hijo
    }

    // El padre espera a que todos los hijos terminen (en este caso, nunca)
    // Esto evita que el proceso padre termine y deje a los hijos como huérfanos.
    for (int i = 0; i < num_puertos; i++) {
        wait(NULL);
    }

    return 0;
}
