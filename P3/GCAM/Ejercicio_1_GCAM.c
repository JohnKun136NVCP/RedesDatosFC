#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/select.h>

#define BUFFER_SIZE 4096
#define NUM_PORTS 3

int PORTS[NUM_PORTS] = {49200, 49201, 49202};

// Función que aplica el cifrado César a un texto
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
    
    if (target_port == my_port) {
        encryptCaesar(text, shift);
        printf("[PORT %d] File processed successfully\n", my_port);
    }

    // Enviar respuesta al cliente
    send(client_sock, response, strlen(response), 0);
    close(client_sock);
}

int main() {
    int server_socks[NUM_PORTS];
    fd_set readfds;
    int max_fd = 0;

    // Crear sockets para todos los puertos
    for (int i = 0; i < NUM_PORTS; i++) {
        // Crear socket
        server_socks[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socks[i] < 0) {
            perror("[-] Error al crear el socket");
            exit(1);
        }

        // Configurar dirección
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(PORTS[i]);

        // Bind
        if (bind(server_socks[i], (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("[-] Error en bind");
            close(server_socks[i]);
            exit(1);
        }

        // Listen
        if (listen(server_socks[i], 5) < 0) {
            perror("[-] Error en listen");
            close(server_socks[i]);
            exit(1);
        }
        
        printf("[*] Server listening on port %d...\n", PORTS[i]);
        
        // Actualizar max_fd
        if (server_socks[i] > max_fd) {
            max_fd = server_socks[i];
        }
    }

    while (1) {
        FD_ZERO(&readfds);
        
        // Agregar todos los sockets de servidor al conjunto
        for (int i = 0; i < NUM_PORTS; i++) {
            FD_SET(server_socks[i], &readfds);
        }

        // Esperar actividad en cualquier socket
        int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("[-] Error en select");
            continue;
        }

        // Revisar cada puerto por conexiones nuevas
        for (int i = 0; i < NUM_PORTS; i++) {
            if (FD_ISSET(server_socks[i], &readfds)) {
                int client_sock = accept(server_socks[i], NULL, NULL);
                if (client_sock < 0) {
                    perror("[-] Error en accept");
                    continue;
                }
                processClientRequest(client_sock, PORTS[i]);
            }
        }
    }

    for (int i = 0; i < NUM_PORTS; i++) {
        close(server_socks[i]);
    }
    
    return 0;
}

