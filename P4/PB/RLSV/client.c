#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>      // Entrada/salida est ́andar (printf, fopen, etc.)
#include <stdlib.h>     // Funciones generales (exit, malloc, etc.)
#include <string.h>     // Manejo de cadenas (strlen, strcpy, etc.)
#include <time.h>

#define BUFFER_SIZE 1024
#define SERVER_IP "192.168.1.76"
#define S01_IP "192.168.1.101"
#define S02_IP "192.168.1.102"
#define S03_IP "192.168.1.103"
#define S04_IP "192.168.1.104"
#define SERVER_PORT 49200

char *alias_ip = SERVER_IP, *file, *alias;

int peticiones(char *server_ip) {
    
    // Dirección del servidor
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);
    serv_addr.sin_port = htons(SERVER_PORT);

    // Crear socket del cliente
    int client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock == -1) {
        perror("[-] Error to create the socket");
        return 1;
    }

    // Conectarse al puerto 49200
    printf("[*] Connecting to %s : %d...\n", server_ip, SERVER_PORT);
    if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("[-] Error to connect");
        close(client_sock);
        return 1;
    }

    // Recibir nuevo puerto
    char buffer_port[50] = {0};
    read(client_sock, buffer_port, sizeof(buffer_port) - 1);
    int new_port = atoi(buffer_port);
    close(client_sock);

    // Reconectar al puerto asignado
    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_port = htons(new_port);
    if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr))
    < 0) {
        perror("[-] Error to connect");
        close(client_sock);
        return 1;
    }
    printf("[+] Reassigned to port %d\n", new_port);

    // Leer archivo
    FILE *fp = fopen(file, "r");
    char file_content[BUFFER_SIZE-5], linea[BUFFER_SIZE];
    if (fp == NULL) {
        perror("[-] Error to open the file");
        return 1;
    }
    fread(file_content, 1, sizeof(file_content)-1, fp);
    fclose(fp);
    // printf("Contenido: %s\n", file_content);
    
    // Crear y mandar mensaje
    char mensaje[BUFFER_SIZE];
    snprintf(mensaje, sizeof(mensaje), "%s\n%s", file, file_content);
    send(client_sock, mensaje, strlen(mensaje), 0);
    // printf("[+][Client] Shift and file was sent: %s\n", mensaje);

    // Recibir mensaje
    char buffer[BUFFER_SIZE] = {0};
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        if (strstr(buffer, "ACCESS GRANTED") != NULL) 
            printf("[+] SERVER RESPONSE: File received and encripted\n");
        if (strstr(buffer, "ACCESS DENIED") != NULL) 
            printf("[-] SERVER RESPONSE: REJECTED\n");
        else
            printf("%s", buffer);
    } 
    else {
        printf("[-] Server connection tiemeout\n");
    }
	
    // Escribe el archivo
    fp = fopen("info_state.txt", "wb");
    if (fp == NULL) {
        perror("[-] Error to open the file");
        return 1;
    }
	fputs(buffer, fp);
	fclose(fp);
	// Libera los recursos de red.
    close(client_sock);
    return 0;
}

int main(int argc, char *argv[]) {
    // Verificar parámetros
    if (argc != 3) {
        printf("Type: %s <FILE> <SERVER> \n", argv[0]);
        return 1;
    }
    file = argv[1];
    alias = argv[2];
    if(strcmp(alias, "s01") == 0) alias_ip = S01_IP;
    if(strcmp(alias, "s02") == 0) alias_ip = S02_IP;
    if(strcmp(alias, "s03") == 0) alias_ip = S03_IP;
    if(strcmp(alias, "s04") == 0) alias_ip = S04_IP;
    if(alias_ip == SERVER_IP) {
        printf("Invalid name of server: %s \n", alias);
        return 1;
    }

    if(strcmp(alias, "s01") != 0) peticiones(S01_IP);
    if(strcmp(alias, "s02") != 0) peticiones(S02_IP);
    if(strcmp(alias, "s03") != 0) peticiones(S03_IP);
    if(strcmp(alias, "s04") != 0) peticiones(S04_IP);

    return 0;

}