#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>      // Entrada/salida est ́andar (printf, fopen, etc.)
#include <stdlib.h>     // Funciones generales (exit, malloc, etc.)
#include <string.h>     // Manejo de cadenas (strlen, strcpy, etc.)

typedef enum {
    PORT1 = 49200,
    PORT2 = 49201,
    PORT3 = 49202
} Ports;
#define BUFFER_SIZE 1024

int peticion(int given_port, char mensaje[], struct sockaddr_in serv_addr) {
    // Crear socket del cliente
    int client_sock;
    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock == -1) {
        perror("[-] Error to create the socket");
        return 1;
    }
    // Conectarse al servidor
    serv_addr.sin_port = htons(given_port);
    printf("[*] Connecting to PORT %d...\n", given_port);
    if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr))
    < 0) {
        perror("[-] Error to connect");
        close(client_sock);
        return 1;
    }
    // Mandar mensaje
    send(client_sock, mensaje, strlen(mensaje), 0);
    // printf("[+][Client] Shift and file was sent: %s\n", mensaje);
    // Recibir mensaje
    char buffer[BUFFER_SIZE] = {0};
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        if (strstr(buffer, "ACCESS GRANTED") != NULL) 
            printf("[+] SERVER RESPONSE %d: File received and encripted\n", given_port);
        if (strstr(buffer, "ACCESS DENIED") != NULL) 
            printf("[-] SERVER RESPONSE %d: REJECTED\n", given_port);
    } 
    else {
        printf("[-] Server connection tiemeout\n");
    }
    close(client_sock);
    return 0;
}

int main(int argc, char *argv[]) {
    // Verificar parámetros
    if (argc != 5) {
        printf("Type: %s <IP> <server port> <shift> <file>\n", argv[0]);
        return 1;
    }
    char *server_ip = argv[1]; // IP de Ubuntu Server
    int server_port = atoi(argv[2]);
    int shift = atoi(argv[3]) % 26;
    char *file = argv[4];
    if(server_port != PORT1 && server_port != PORT2 && server_port != PORT3) {
        printf("Invalid port: %s\n", argv[2]);
        return 1;
    }

    // Leer archivo
    FILE *fp = fopen(file, "r");
    char file_content[BUFFER_SIZE-10], linea[BUFFER_SIZE-10];
    if (fp == NULL) {
        perror("[-] Error to open the file");
        return 1;
    }
    while(fgets(linea, BUFFER_SIZE-10, fp)) 
        strcat(file_content, linea);
    fclose(fp);

    // Crear mensaje
    char mensaje[BUFFER_SIZE];
    snprintf(mensaje, sizeof(mensaje), "%d %d %s", shift, server_port, file_content);

    // Dirección del servidor
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);

    // Conectarse a servidores
    int close_client = 0;
    for(int p = PORT1; p<=PORT3; p++) {
        close_client += peticion(p, mensaje, serv_addr);
    }
}