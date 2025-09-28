#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define BUFFER_SIZE 1024 // Tamaño del buffer para recibir datos

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

void toLowerCase(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower((unsigned char)str[i]);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2){
        printf("Forma de uso: %s <PUERTO>\n", argv[0]);
        return 1;
    }

    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    char buffer[BUFFER_SIZE] = {0};
    char clave[BUFFER_SIZE];
    char *port = argv[1];
    int shift;
    int client_port;

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("[-] Error to create the socket");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(port));
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] Error binding");
        close(server_sock);
        return 1;
    }

    if (listen(server_sock, 1) < 0) {
        perror("[-] Error on listen");
        close(server_sock);
        return 1;
    }

    printf("[+] Servidor a la escucha en el puerto %s...\n", port);

    addr_size = sizeof(client_addr);
    client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
    if (client_sock < 0) {
        perror("[-] Error on accept");
        close(server_sock);
        return 1;
    }

    printf("[+] Conexion con el cliente establecida\n");

    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        printf("[-] Missed key\n");
        close(client_sock);
        close(server_sock);
        return 1;
    }

    buffer[bytes] = '\0';
    sscanf(buffer, "%i %d", &client_port, &shift);
    printf("[+][Server] Intento de conexion al puerto: %d\n", client_port);

    if(client_port != atoi(port)){
        send(client_sock, "ACCESO DENEGADO", strlen("ACCESO DENEGADO"), 0);
        printf("[-][Server] El cliente intenta conectarse al puerto %d, no al puerto %s\n", client_port, port);
        close(server_sock);
        close(client_sock);
    }else{
        printf("[+][Server] Conexión establecida con el cliente en puerto %d\n", client_port);
        send(client_sock, "ACCESO ACEPTADO", strlen("ACCESO ACEPTADO"), 0);
        sleep(1);

        char buffer[BUFFER_SIZE];
        int bytes;
        char mensaje[BUFFER_SIZE * 10];
        int total = 0;

        while ((bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
            buffer[bytes] = '\0';

            if (total + bytes < sizeof(mensaje) - 1) {
                memcpy(mensaje + total, buffer, bytes);
                total += bytes;
                mensaje[total] = '\0';
            }

            if (strchr(buffer, '\n') != NULL) {
                break;
            }
        }

        if (bytes < 0) {
            perror("[-][Server] Error al recibir mensaje");
        } else if (total > 0) {
            // Quitar el \n final
            mensaje[strcspn(mensaje, "\n")] = '\0';
            printf("[+][Server] Mensaje completo recibido:\n%s\n", mensaje);
        }
        encryptCaesar(mensaje, shift);
        printf("[+][Server] Mensaje encriptado:\n%s\n", mensaje);

        FILE *out = fopen("mensaje_encriptado.txt", "w");
        if (out == NULL) {
            perror("[-][Server] No se pudo crear el archivo");
        } else {
            fprintf(out, "%s\n", mensaje);
            fclose(out);
            printf("[+][Server] Mensaje encriptado guardado en mensaje_encriptado.txt\n");
        }
    }

    close(server_sock);

    return 0;
}
