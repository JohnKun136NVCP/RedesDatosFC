#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

//serverOpt.c

//#define PORT 7006        // Puerto en el que el servidor escucha
#define BUFFER_SIZE 1024 // Tamaño del buffer para recibir datos
int server_ports[] = {49200, 49201, 49202}; // Puertos en los que el servidor escucha
//server.c

/*
    Función para encriptar texto usando el cifrado César
*/
void encryptCaesar(char *text, int shift) {
    // Ajustamos el desplazamiento y verificamos si las letras son mayúsculas o minúsculas
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

/*
    Función principal que nos permite recibir el contenido del archivo que envió el cliente para cifrarlo.
    Se utilizan los 3 puertos al mismo tiempo desde la misma terminal
*/
int main() {
    int ports[3];
    struct sockaddr_in server_addr[3];
    fd_set readfds;
    int max_fd = -1;
    
    // Bandera para controlar cuáles puertos han sido procesados
    int processed[3] = {0, 0, 0};
    int remaining = 3;

    // Creamos los sockets 
    for (int i = 0; i < 3; i++) {
        ports[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (ports[i] < 0) {
            perror("[-] Error creating socket");
            return 1;
        }

        int opt = 1;
        // Permitimos que se vuelva a usar el puerto después de termianr la ejecución del programa
        if (setsockopt(ports[i], SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            perror("setsockopt SO_REUSEADDR failed");
            return 1;
        }
        
        // Configuramos la dirección del servidor
        server_addr[i].sin_family = AF_INET;
        server_addr[i].sin_port = htons(server_ports[i]);
        server_addr[i].sin_addr.s_addr = INADDR_ANY;
    
        // Asignamos el socket a la dirección y puerto especificados
        if (bind(ports[i], (struct sockaddr *)&server_addr[i], sizeof(server_addr[i])) < 0) {
            perror("[-] Error binding");
            close(ports[i]);
            return 1;
        }

        // Escuchamos conexiones entrantes
        if (listen(ports[i], 1) < 0) {
            perror("[-] Error on listen");
            close(ports[i]);
            return 1;
        }

        printf("[*] LISTENING %d...\n", server_ports[i]);

        if (ports[i] > max_fd) {
            max_fd = ports[i];
        }
    }

    struct timeval timeout;
    timeout.tv_sec = 10;  
    timeout.tv_usec = 0;

    while (remaining > 0) {
        FD_ZERO(&readfds);
        
        // Monitoreamos los puertos que no han sido procesados
        for (int i = 0; i < 3; i++) {
            if (!processed[i]) {
                FD_SET(ports[i], &readfds);
            }
        }

        int activity = select(max_fd + 1, &readfds, NULL, NULL, &timeout);

        // Rechazamos los puertos pendientes si no hay actividad
        if (activity == 0) {
            printf("[+] Timeout reached. Rejecting remaining ports...\n");
            for (int i = 0; i < 3; i++) {
                if (!processed[i]) {
                    printf("[SERVER %d] REJECTED\n", server_ports[i]);
                    processed[i] = 1;
                    remaining--;
                }
            }
            break;
        }
        
        if (activity < 0) {
            perror("select error");
            continue;
        }

        for (int i = 0; i < 3; i++) {
            // Procesamos los puertos que no han sido atendidos
            if (!processed[i] && FD_ISSET(ports[i], &readfds)) {
                struct sockaddr_in client_addr;
                socklen_t addr_size = sizeof(client_addr);
                int client_sock = accept(ports[i], (struct sockaddr*)&client_addr, &addr_size);
                if (client_sock < 0) {
                    perror("Accept error");
                    continue;
                }

                char buffer[BUFFER_SIZE] = {0};
                char file_content[BUFFER_SIZE] = {0};
                int requested_port, shift;

                // Recibimos la solicitud del cliente
                int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
                if (bytes > 0) {
                    buffer[bytes] = '\0';

                    // Validamos el formato de la solicitud
                    if (sscanf(buffer, "%d|%d|%[^\n]", &requested_port, &shift, file_content) == 3) {
                        if (requested_port == server_ports[i] && shift == 34) {
                            encryptCaesar(file_content, shift);
                            char *msg = "File received and encrypted";
                            send(client_sock, msg, strlen(msg), 0);
                            printf("[SERVER %d] File encrypted:\n%s\n", server_ports[i], file_content);
                        } else {
                            char *msg = "REJECTED\n";
                            send(client_sock, msg, strlen(msg), 0);
                            printf("[SERVER %d] REJECTED\n", 
                                   server_ports[i]);
                        }
                    } else {
                        char *msg = "REJECTED\n";
                        send(client_sock, msg, strlen(msg), 0);
                        printf("[SERVER %d] Invalid format. REJECTED\n", server_ports[i]);
                    }
                } else {
                    char *msg = "REJECTED\n";
                    send(client_sock, msg, strlen(msg), 0);
                    printf("[SERVER %d] No data received. REJECTED\n", server_ports[i]);
                }

                close(client_sock);
                
                processed[i] = 1;
                remaining--;
            }
        }
        
        // Reseteamos el timeout para la siguiente iteración
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
    }
    
    for (int i = 0; i < 3; i++) {
        close(ports[i]);
    }
    return 0;
}
