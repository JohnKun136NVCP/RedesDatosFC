#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define PORT 49202
#define BUFFER_SIZE 1024

void decryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            text[i] = ((c - 'A' - shift + 26) % 26) + 'A';
        } else if (islower(c)) {
            text[i] = ((c - 'a' - shift + 26) % 26) + 'a';
        } else {
            text[i] = c;
        }
    }
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    char buffer[BUFFER_SIZE * 2] = {0};
    char mensaje_cifrado[BUFFER_SIZE] = {0};
    int shift;

    // Crear socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("[-] Error to create the socket");
        return 1;
    }

    printf("[+] Socket created successfully\n");

    // Configurar dirección del servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind socket al puerto
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] Error binding");
        close(server_sock);
        return 1;
    }

    printf("[+] Socket bound to port %d\n", PORT);

    // Poner en modo escucha
    if (listen(server_sock, 5) < 0) {
        perror("[-] Error on listen");
        close(server_sock);
        return 1;
    }

    printf("[+] Server listening on port %d (ACCEPTANCE SERVER)...\n", PORT);

    while (1) {
        // Aceptar conexión entrante
        addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);

        if (client_sock < 0) {
            perror("[-] Error on accept");
            continue;
        }

        // Obtener información del cliente
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(client_addr.sin_port);

        printf("[+] Client connected from %s:%d to port %d\n", client_ip, client_port, PORT);

        // Recibir mensaje del cliente (mensaje cifrado + shift)
        int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);

        if (bytes <= 0) {
            printf("[-] No message received from client\n");
            close(client_sock);
            continue;
        }

        buffer[bytes] = '\0';
        printf("[+][Server] Received data: %s\n", buffer);

        // Separar el mensaje cifrado del shift
        char *token = strtok(buffer, "|");
        if (token != NULL) {
            strcpy(mensaje_cifrado, token);
            token = strtok(NULL, "|");
            if (token != NULL) {
                shift = atoi(token);
            } else {
                printf("[-] Error: Shift value not found\n");
                send(client_sock, "REJECTED - Invalid format", strlen("REJECTED - Invalid format"), 0);
                close(client_sock);
                continue;
            }
        } else {
            printf("[-] Error: Invalid message format\n");
            send(client_sock, "REJECTED - Invalid format", strlen("REJECTED - Invalid format"), 0);
            close(client_sock);
            continue;
        }

        printf("[+][Server] Encrypted message: %s\n", mensaje_cifrado);
        printf("[+][Server] Shift value: %d\n", shift);

        // Descifrar el mensaje
        decryptCaesar(mensaje_cifrado, shift);
        printf("[+][Server] Decrypted message: %s\n", mensaje_cifrado);

        // Enviar confirmación al cliente
        char acceptance_msg[100];
        snprintf(acceptance_msg, sizeof(acceptance_msg), "SERVER RESPONSE %d: File received and encrypted.", PORT);
        send(client_sock, acceptance_msg, strlen(acceptance_msg), 0);

        printf("[+] ACCEPTED client connection to port %d\n", PORT);

        // Cerrar conexión del cliente
        close(client_sock);
        printf("[+] Client disconnected from port %d\n\n", PORT);
    }

    close(server_sock);
    return 0;
}