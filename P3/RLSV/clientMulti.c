#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>      // Entrada/salida est ́andar (printf, fopen, etc.)
#include <stdlib.h>     // Funciones generales (exit, malloc, etc.)
#include <string.h>     // Manejo de cadenas (strlen, strcpy, etc.)

#define BUFFER_SIZE 1024

char *server_ip;
int shift;

int peticion(int server_port, char *file) {

    // Verificar puerto
    if(server_port < 49200 || server_port > 49202) {
        printf("Invalid port: %d\n", server_port);
        return 1;
    }

    // Leer archivo
    FILE *fp = fopen(file, "r");
    char file_content[BUFFER_SIZE], linea[BUFFER_SIZE];
    if (fp == NULL) {
        perror("[-] Error to open the file");
        return 1;
    }
    fread(file_content, 1, sizeof(file_content)-1, fp);
    fclose(fp);

    // printf("Contenido: %s\n", file_content);

    // Crear mensaje
    char mensaje[BUFFER_SIZE];
    snprintf(mensaje, sizeof(mensaje), "%d %d %s", shift, server_port, file_content);

    // Dirección del servidor
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);

    // Crear socket del cliente
    int client_sock;
    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock == -1) {
        perror("[-] Error to create the socket");
        return 1;
    }

    // Conectarse al servidor
    serv_addr.sin_port = htons(server_port);
    printf("[*] Connecting to PORT %d...\n", server_port);
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
            printf("[+] SERVER RESPONSE %d: File received and encripted\n", server_port);
        if (strstr(buffer, "ACCESS DENIED") != NULL) 
            printf("[-] SERVER RESPONSE %d: REJECTED\n", server_port);
    } 
    else {
        printf("[-] Server connection tiemeout\n");
    }
    close(client_sock);
    return 0;
}

int main(int argc, char *argv[]) {
    // Verificar parámetros
    if (argc < 5 || argc%2==0) {
        printf("Type: %s <IP> <PORTS> <FILES> <SHIFT>\n", argv[0]);
        return 1;
    }
    server_ip = argv[1]; // IP de Ubuntu Server
    shift = atoi(argv[argc-1]) % 26;
    
    // Conectarse a servidores
    for(int i=0; i<(argc-3)/2; i++) {
        peticion(atoi(argv[2+i]), argv[2+(argc-3)/2+i]);
    }
}