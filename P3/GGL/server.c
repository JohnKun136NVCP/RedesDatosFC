#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define BUFFER_SIZE 1024

void encryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            text[i] = ((c - 'A' + shift) % 26) + 'A';
        } else if (islower(c)) {
            text[i] = ((c - 'a' + shift) % 26) + 'a';
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <puerto>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    char buffer[BUFFER_SIZE] = {0};

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("[-] Error al crear socket");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] Error en bind");
        close(server_sock);
        return 1;
    }

    if (listen(server_sock, 1) < 0) {
        perror("[-] Error en listen");
        close(server_sock);
        return 1;
    }

    printf("[+] Servidor escuchando en puerto %d...\n", port);

    addr_size = sizeof(client_addr);
    client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
    if (client_sock < 0) {
        perror("[-] Error en accept");
        close(server_sock);
        return 1;
    }

    printf("[+] Cliente conectado\n");

    // Recibir datos del cliente
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        printf("[-] Error al recibir datos\n");
        close(client_sock);
        close(server_sock);
        return 1;
    }
    buffer[bytes] = '\0';

    // Parsear: <PUERTO_OBJETIVO> <DESPLAZAMIENTO> <TEXTO>
    int target_port, shift;
    char text[BUFFER_SIZE];
    sscanf(buffer, "%d %d %[^\n]", &target_port, &shift, text);

    printf("[+] Puerto objetivo: %d\n", target_port);
    printf("[+] Desplazamiento: %d\n", shift);
    printf("[+] Texto recibido: %s\n", text);

    if (target_port == port) {
        // Aplicar cifrado César
        encryptCaesar(text, shift);
        printf("[+] Texto cifrado: %s\n", text);
        send(client_sock, "PROCESADO Y ENCRIPTANDO", 9, 0);
    } else {
        printf("[-] Puerto no coincide. Rechazado.\n");
        send(client_sock, "RECHAZADO", 9, 0);
    }

    close(client_sock);
    close(server_sock);
    return 0;
}