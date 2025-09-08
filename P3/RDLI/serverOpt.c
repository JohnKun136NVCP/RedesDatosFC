#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
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

int create_listen_socket(int port) {
    int sock;
    struct sockaddr_in addr;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("[-] Error to create the socket");
        exit(1);
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[-] Error binding");
        close(sock);
        exit(1);
    }
    if (listen(sock, 5) < 0) {
        perror("[-] Error on listen");
        close(sock);
        exit(1);
    }
    printf("[+] Server listening on port %d...\n", port);
    return sock;
}

int main() {
    //Multiport server
    int ports[3] = {49200, 49201, 49202};
    int listen_socks[3];
    for (int i = 0; i < 3; i++) {
        listen_socks[i] = create_listen_socket(ports[i]);
    }

    fd_set readfds;
    int maxfd = listen_socks[0];
    for (int i = 1; i < 3; i++)
        if (listen_socks[i] > maxfd) maxfd = listen_socks[i];

    while (1) {
        FD_ZERO(&readfds);
        for (int i = 0; i < 3; i++)
            FD_SET(listen_socks[i], &readfds);

        int activity = select(maxfd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("select error");
            break;
        }

        for (int i = 0; i < 3; i++) {
            if (FD_ISSET(listen_socks[i], &readfds)) {
                struct sockaddr_in client_addr;
                socklen_t addr_size = sizeof(client_addr);
                int client_sock = accept(listen_socks[i], (struct sockaddr *)&client_addr, &addr_size);
                if (client_sock < 0) {
                    perror("[-] Error on accept");
                    continue;
                }

                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
                int client_port = ntohs(client_addr.sin_port);

                printf("[+] Client connected from %s:%d to port %d\n", client_ip, client_port, ports[i]);

                char buffer[BUFFER_SIZE * 2] = {0};
                char mensaje_cifrado[BUFFER_SIZE] = {0};
                int shift = 0;

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
                snprintf(acceptance_msg, sizeof(acceptance_msg), "SERVER RESPONSE %d: File received and encrypted.", ports[i]);
                send(client_sock, acceptance_msg, strlen(acceptance_msg), 0);

                printf("[+] ACCEPTED client connection to port %d\n", ports[i]);
                close(client_sock);
                printf("[+] Client disconnected from port %d\n\n", ports[i]);
            }
        }
    }

    for (int i = 0; i < 3; i++)
        close(listen_socks[i]);
    return 0;
}