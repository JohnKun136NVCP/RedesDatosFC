#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define BUFFER_SIZE 4096

// Función que aplica el cifrado César
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

// Atiende la solicitud de un cliente
void processClientRequest(int client_sock, int my_port) {
    char buffer[BUFFER_SIZE];
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);

    if (bytes <= 0) {
        printf("[-] No data received\n");
        close(client_sock);
        return;
    }
    buffer[bytes] = '\0';

    // Extraer partes del mensaje
    char *port_str = strtok(buffer, "|");
    char *shift_str = strtok(NULL, "|");
    char *text = strtok(NULL, "");

    if (!port_str || !shift_str || !text) {
        const char *error_msg = "Error: invalid format\n";
        send(client_sock, error_msg, strlen(error_msg), 0);
        close(client_sock);
        return;
    }

    int target_port = atoi(port_str);
    int shift = atoi(shift_str);

    char response[BUFFER_SIZE];
    char encrypted_text[BUFFER_SIZE];
    
    if (target_port == my_port) {
        strcpy(encrypted_text, text);

        // Aplicar cifrado César
        encryptCaesar(encrypted_text, shift);

        // Mostrar solo el mensaje cifrado en el servidor 
        printf("[SERVER %d] Encrypted text: %s\n", my_port, encrypted_text);

        // Enviar el texto cifrado al cliente
        int response_length = snprintf(response, sizeof(response), 
                 "File received and encrypted:\n%s\n", encrypted_text);

        if (response_length >= sizeof(response)) {
            strncpy(response, "File received and encrypted (truncated):\n", sizeof(response) - 1);
            strncat(response, encrypted_text, sizeof(response) - strlen(response) - 1);
            response[sizeof(response) - 1] = '\0';
        }

        printf("[SERVER %d] File processed successfully\n", my_port);
    } else {
        snprintf(response, sizeof(response), "REJECTED\n");
        printf("[SERVER %d] Request rejected (client requested port %d)\n", 
               my_port, target_port);
    }

    // Enviar respuesta al cliente
    send(client_sock, response, strlen(response), 0);
    close(client_sock);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "USE: %s <PORT>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int PORT = atoi(argv[1]);
    int server_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;

    // Crear el socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("[-] Failed to create socket");
        return 1;
    }

    // Configurar dirección
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] Failed to bind");
        close(server_sock);
        return 1;
    }

    if (listen(server_sock, 5) < 0) {
        perror("[-] Failed to listen");
        close(server_sock);
        return 1;
    }
    printf("[*] Server listening on port %d...\n", PORT);

    while (1) {
        addr_size = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
        if (client_sock < 0) {
            perror("[-] Failed to accept connection");
            continue;
        }
        printf("[+] Client connected\n");

        processClientRequest(client_sock, PORT);
    }

    close(server_sock);
    return 0;
}
