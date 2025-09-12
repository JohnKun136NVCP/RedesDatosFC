//ClientMulti.c.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 2048

int main(int argc, char *argv[]) {
    // Se espera: argv[1] = IP servidor
    // Luego grupos de 3 argumentos: <puerto> <shift> <archivo>
    if (argc < 5 || ((argc - 2) % 3) != 0) {
        printf("Uso: %s <IP_servidor> <puerto1> <shift1> <archivo1> [<puerto2> <shift2> <archivo2> ...]\n", argv[0]);
        return 1;
    }

    char *ip_servidor = argv[1];
    int num_grupos = (argc - 2) / 3;

    for (int i = 0; i < num_grupos; i++) {
        int idx = 2 + i * 3;
        int puerto = atoi(argv[idx]);
        int shift = atoi(argv[idx + 1]);
        char *archivo = argv[idx + 2];

        // Abrir archivo
        FILE *fp = fopen(archivo, "r");
        if (fp == NULL) {
            perror("Error al abrir el archivo");
            continue;  // saltar a la siguiente tripleta
        }

        char texto[BUFFER_SIZE] = "";
        size_t bytes_read = fread(texto, 1, sizeof(texto) - 1, fp);
        fclose(fp);

        texto[bytes_read] = '\0';

        int sockfd;
        struct sockaddr_in serv_addr;
        char buffer[BUFFER_SIZE];

        // Crear socket TCP
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("Error al crear socket");
            continue;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(puerto);

        if (inet_pton(AF_INET, ip_servidor, &serv_addr.sin_addr) <= 0) {
            perror("Dirección inválida");
            close(sockfd);
            continue;
        }

        if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            perror("Error al conectar");
            close(sockfd);
            continue;
        }

        // Formatear mensaje: "<puerto> <shift> <texto>"
        snprintf(buffer, sizeof(buffer), "%d %d %s", puerto, shift, texto);

        // Enviar mensaje
        send(sockfd, buffer, strlen(buffer), 0);

        // Recibir respuesta
        int bytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            printf("Respuesta del servidor en puerto %d: %s\n", puerto, buffer);
        } else {
            printf("No se recibió respuesta del servidor en puerto %d\n", puerto);
        }

        close(sockfd);
    }

    return 0;
}
