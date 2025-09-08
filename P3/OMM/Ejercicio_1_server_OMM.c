#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>

#define BUFFER_SIZE 1024

void encryptCaesar(char *text, int shift) {
    for (int i = 0; text[i] != '\0'; i++) {
        if (isupper(text[i])) {
            text[i] = 'A' + (text[i] - 'A' + shift) % 26;
        } else if (islower(text[i])) {
            text[i] = 'a' + (text[i] - 'a' + shift) % 26;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <puerto>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Error al crear socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error al enlazar socket");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Error al escuchar");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Servidor escuchando en puerto %d...\n", port);

    while (1) {
        if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len)) < 0) {
            perror("Error al aceptar conexión");
            continue;
        }

        printf("Cliente conectado\n");

        int bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
        if (bytes_received < 0) {
            perror("Error al recibir datos");
            close(client_fd);
            continue;
        }

        buffer[bytes_received] = '\0';

        int target_port, shift;
        char content[BUFFER_SIZE];
        sscanf(buffer, "%d %d %[^\n]", &target_port, &shift, content);

        printf("Solicitud recibida - Puerto objetivo: %d, Desplazamiento: %d\n", target_port, shift);

        if (target_port == port) {
            printf("Procesando archivo...\n");
            encryptCaesar(content, shift % 26);
            printf("Contenido cifrado: %s\n", content);  
            send(client_fd, "PROCESADO", 9, 0);
            printf("Archivo cifrado y procesado\n");
        } else {
            printf("Rechazando solicitud (puerto no coincide)\n");
            send(client_fd, "RECHAZADO", 9, 0);
        }

        close(client_fd);
    }

    close(server_fd);
    return 0;
}
