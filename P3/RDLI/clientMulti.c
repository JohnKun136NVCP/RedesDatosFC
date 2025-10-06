#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
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
        } else {
            text[i] = c;
        }
    }
}

int main(int argc, char *argv[]) {
    // ./clientMulti <server_ip> <port1> <shift1> <file1> [<port2> <shift2> <file2> ...]
    if (argc < 5 || ((argc - 2) % 3 != 0)) {
        printf("Usage: %s <server_ip> <port1> <shift1> <file1> [<port2> <shift2> <file2> ...]\n", argv[0]);
        return 1;
    }

    char *server_ip = argv[1];
    int num_targets = (argc - 2) / 3; // Número de conjuntos (port, shift, file)

    // Para cada conjunto, crear una conexión y enviar el mensaje
    for (int t = 0; t < num_targets; t++) {
        int arg_idx = 2 + t * 3;
        int PORT = atoi(argv[arg_idx]);
        int shift = atoi(argv[arg_idx + 1]);
        char *filename = argv[arg_idx + 2];

        printf("\n[+] Connecting to %s:%d with shift %d and file %s\n", server_ip, PORT, shift, filename);

        int client_sock;
        struct sockaddr_in serv_addr;
        char buffer[BUFFER_SIZE] = {0};
        char mensaje[BUFFER_SIZE] = {0};
        char mensaje_con_shift[BUFFER_SIZE * 2] = {0};

        client_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (client_sock == -1) {
            perror("[-] Error to create the socket");
            continue;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT);
        if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
            perror("[-] Invalid address");
            close(client_sock);
            continue;
        }

        if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            perror("[-] Error to connect");
            printf("errno: %d (%s)\n", errno, strerror(errno));
            close(client_sock);
            continue;
        }

        printf("[+] Connected to server\n");

        FILE *fp = fopen(filename, "r");
        if (fp == NULL) {
            perror("[-] Error to open the file");
            close(client_sock);
            continue;
        }

        size_t bytes_read = fread(mensaje, sizeof(char), BUFFER_SIZE - 1, fp);
        mensaje[bytes_read] = '\0';
        fclose(fp);

        printf("[+] Original message: %s\n", mensaje);

        encryptCaesar(mensaje, shift);
        printf("[+] Encrypted message: %s\n", mensaje);

        snprintf(mensaje_con_shift, sizeof(mensaje_con_shift), "%s|%d", mensaje, shift);
        send(client_sock, mensaje_con_shift, strlen(mensaje_con_shift), 0);

        printf("[+][Client] Sent encrypted message with shift: %s\n", mensaje_con_shift);

        int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            printf("[+][Client] Server response: %s\n", buffer);
        } else {
            printf("[-] No response from server\n");
        }

        close(client_sock);
    }
}