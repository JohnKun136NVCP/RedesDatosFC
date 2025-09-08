#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <libgen.h>

#define BUFFER_SIZE 4096

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Uso: %s <SERVIDOR_IP> <PUERTO1> <PUERTO2> <PUERTO3> <ARCHIVO1> <ARCHIVO2> <ARCHIVO3> <DESPLAZAMIENTO>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *server_ip = argv[1];
    int ports[3];
    ports[0] = atoi(argv[2]);
    ports[1] = atoi(argv[3]);
    ports[2] = atoi(argv[4]);
    
    char *filenames[3];
    filenames[0] = argv[5];
    filenames[1] = argv[6];
    filenames[2] = argv[7];
    
    int shift = atoi(argv[8]);

    for (int i = 0; i < 3; i++) {
        FILE *file = fopen(filenames[i], "r");
        if (!file) {
            fprintf(stderr, "Error al abrir archivo: %s\n", filenames[i]);
            continue;
        }

        char content[BUFFER_SIZE];
        size_t bytes_read = fread(content, 1, BUFFER_SIZE - 1, file);
        content[bytes_read] = '\0';
        fclose(file);

        char filename_copy[256];
        strncpy(filename_copy, filenames[i], sizeof(filename_copy));
        char *base_name = basename(filename_copy);

        int sock;
        struct sockaddr_in serv_addr;
        char buffer[BUFFER_SIZE] = {0};

        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("Error al crear socket");
            continue;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(ports[i]);

        if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
            perror("Dirección inválida");
            close(sock);
            continue;
        }

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            fprintf(stderr, "Conexión fallida al puerto %d\n", ports[i]);
            close(sock);
            continue;
        }

        char message[BUFFER_SIZE * 2];
        snprintf(message, sizeof(message), "%d %d %s\n%s", ports[i], shift, base_name, content);
        
        if (send(sock, message, strlen(message), 0) < 0) {
            perror("Error al enviar datos");
            close(sock);
            continue;
        }

        printf("[cliente] Datos enviados al puerto %d\n", ports[i]);

        int bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            printf("[cliente] Puerto %d: %s\n", ports[i], buffer);
        } else {
            printf("[cliente] Puerto %d: Sin respuesta\n", ports[i]);
        }

        close(sock);
    }

    return 0;
}
