#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/select.h>

#define BUFFER_SIZE 1024 // Tamaño del buffer para recibir datos
#define MAX_PORTS 10     // Máximo de puertos soportados

void encryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            text[i] = ((c - 'A' + shift) % 26) + 'A';
        } else if (islower(c)) {
            text[i] = ((c - 'a' + shift) % 26) + 'a';
        } else {
            text[i] = c;
        }
    }
}

void handle_client(int client_sock, int server_port) {
    char buffer[BUFFER_SIZE] = {0};
    int client_port, shift;

    // Recibir clave inicial
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        printf("[-] Missed key\n");
        close(client_sock);
        return;
    }
    buffer[bytes] = '\0';
    sscanf(buffer, "%i %d", &client_port, &shift);

    printf("[+][Server] Intento de conexión al puerto: %d\n", client_port);

    if (client_port != server_port) {
        send(client_sock, "ACCESO DENEGADO", strlen("ACCESO DENEGADO"), 0);
        printf("[-][Server] El cliente intenta conectarse al puerto %d, no al puerto %d\n",
               client_port, server_port);
        close(client_sock);
        return;
    }

    printf("[+][Server] Conexión establecida con el cliente en puerto %d\n", client_port);
    send(client_sock, "ACCESO ACEPTADO", strlen("ACCESO ACEPTADO"), 0);
    sleep(1);

    // Recibir mensaje completo
    char mensaje[BUFFER_SIZE * 10];
    int total = 0;
    while ((bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes] = '\0';
        if (total + bytes < sizeof(mensaje) - 1) {
            memcpy(mensaje + total, buffer, bytes);
            total += bytes;
            mensaje[total] = '\0';
        }
        if (strchr(buffer, '\n') != NULL) break;
    }

    if (bytes < 0) {
        perror("[-][Server] Error al recibir mensaje");
    } else if (total > 0) {
        mensaje[strcspn(mensaje, "\n")] = '\0';
        printf("[+][Server] Mensaje completo recibido:\n%s\n", mensaje);

        encryptCaesar(mensaje, shift);
        printf("[+][Server] Mensaje encriptado:\n%s\n", mensaje);

        char filename[64];
        snprintf(filename, sizeof(filename), "mensaje_encriptado_%d.txt", client_port);

        FILE *out = fopen(filename, "w");
        if (out == NULL) {
            perror("[-][Server] No se pudo crear el archivo");
        } else {
            fprintf(out, "%s\n", mensaje);
            fclose(out);
            printf("[+][Server] Mensaje encriptado guardado en %s\n", filename);
        }
    }

    close(client_sock);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Forma de uso: %s <PUERTO1> <PUERTO2> ... <PUERTO_N>\n", argv[0]);
        return 1;
    }

    int num_ports = argc - 1;
    if (num_ports > MAX_PORTS) {
        printf("[-] Máximo de puertos soportados: %d\n", MAX_PORTS);
        return 1;
    }

    int ports[MAX_PORTS];
    int server_socks[MAX_PORTS];
    fd_set readfds;
    int max_fd = 0;

    // Crear y configurar sockets para cada puerto
    for (int i = 0; i < num_ports; i++) {
        ports[i] = atoi(argv[i + 1]);
        server_socks[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socks[i] == -1) {
            perror("[-] Error al crear socket");
            return 1;
        }

        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(ports[i]);

        if (bind(server_socks[i], (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("[-] Error en bind");
            return 1;
        }

        if (listen(server_socks[i], 5) < 0) {
            perror("[-] Error en listen");
            return 1;
        }

        if (server_socks[i] > max_fd) max_fd = server_socks[i];
        printf("[+] Servidor a la escucha en puerto %d...\n", ports[i]);
    }

    // Bucle principal con select()
    while (1) {
        FD_ZERO(&readfds);
        for (int i = 0; i < num_ports; i++) {
            FD_SET(server_socks[i], &readfds);
        }

        int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("[-] Error en select");
            break;
        }

        for (int i = 0; i < num_ports; i++) {
            if (FD_ISSET(server_socks[i], &readfds)) {
                struct sockaddr_in client_addr;
                socklen_t addrlen = sizeof(client_addr);

                int client_sock = accept(server_socks[i], (struct sockaddr *)&client_addr, &addrlen);
                if (client_sock >= 0) {
                    printf("[+] Cliente conectado en puerto %d\n", ports[i]);
                    handle_client(client_sock, ports[i]);
                }
                printf("============================================\n\n");
            }
        }
    }

    for (int i = 0; i < num_ports; i++) {
        close(server_socks[i]);
    }

    return 0;
}
