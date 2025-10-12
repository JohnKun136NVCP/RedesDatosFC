#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h> 

#define PORT 49200
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    // Uso: ./clientMultiPA <ip_servidor> <alias_cliente> <archivo>
    if (argc < 4) {
        printf("Uso: %s <ip_servidor> <alias_cliente> <archivo>\n", argv[0]);
        return -1;
    }

    char *ip_servidor = argv[1];    
    char *alias_cliente = argv[2];  
    char *archivo = argv[3];        

    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error al crear socket");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, ip_servidor, &serv_addr.sin_addr) <= 0) {
        perror("Dirección IP inválida");
        close(sock);
        return -1;
    }

    // conexión al servidor
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Error en la conexión");
        close(sock);
        return -1;
    }
    printf("[%s] conexión establecida con éxito a %s:%d\n", alias_cliente, ip_servidor, PORT);
    
    // enviar alias y nombre del archivo (incluyendo el \0 para delimitador)
    send(sock, alias_cliente, strlen(alias_cliente) + 1, 0); 
    send(sock, archivo, strlen(archivo) + 1, 0); 

    // abrir y enviar contenido del archivo
    FILE *fp = fopen(archivo, "r");
    if (!fp) {
        perror("Error al abrir archivo local");
        close(sock);
        return -1;
    }
    
    ssize_t bytes_read;
    long total_bytes_sent = 0;

    printf("[%s] iniciando envío del archivo: %s\n", alias_cliente, archivo);
    
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        send(sock, buffer, bytes_read, 0);
        total_bytes_sent += bytes_read;
    }
    fclose(fp);

    // notificar al servidor que la escritura terminó (crucial para liberar el mutex)
    shutdown(sock, SHUT_WR); 
    printf("[%s] archivo enviado. total bytes: %ld. esperando respuesta...\n", alias_cliente, total_bytes_sent);

    // leer la respuesta del servidor (éxito o rechazo por turno/ocupación)
    memset(buffer, 0, BUFFER_SIZE);
    if (read(sock, buffer, BUFFER_SIZE) > 0) {
        printf("<<< respuesta servidor >>>: %s\n", buffer);
    }

    close(sock);
    return 0;
}