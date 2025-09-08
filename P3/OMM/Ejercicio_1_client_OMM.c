#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Uso: %s <SERVIDOR_IP> <PUERTO> <DESPLAZAMIENTO> <archivo>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *server_ip = argv[1];
    int port = atoi(argv[2]);
    int shift = atoi(argv[3]);
    char *filename = argv[4];

    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error al abrir archivo");
        exit(EXIT_FAILURE);
    }

    char content[BUFFER_SIZE];
    fgets(content, BUFFER_SIZE, file);
    fclose(file);

    int ports[] = {49200, 49201, 49202};
    int num_ports = sizeof(ports) / sizeof(ports[0]);

    for (int i = 0; i < num_ports; i++) {
        int current_port = ports[i];
        int sock;
        struct sockaddr_in serv_addr;
        char buffer[BUFFER_SIZE] = {0};

        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("Error al crear socket");
            continue;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(current_port);

        if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
            perror("Dirección inválida");
            close(sock);
            continue;
        }

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            printf("Conexión fallida al puerto %d\n", current_port);
            close(sock);
            continue;
        }

        printf("Conectado al servidor en puerto %d\n", current_port);

        char message[BUFFER_SIZE];
        snprintf(message, sizeof(message), "%d %d %s", port, shift, content);
        send(sock, message, strlen(message), 0);
        printf("Datos enviados al puerto %d\n", current_port);

        int bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            printf("Respuesta del puerto %d: %s\n", current_port, buffer);
        }

        close(sock);
    }

    return 0;
}
