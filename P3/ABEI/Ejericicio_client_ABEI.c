#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Uso: %s <SERVIDOR_IP> <PUERTO> <DESPLAZAMIENTO> <ARCHIVO>\n", argv[0]);
        return 1;
    }

    char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    int shift = atoi(argv[3]);
    char *filename = argv[4];

    int sock;
    struct sockaddr_in serv_addr;
    FILE *fp;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read, bytes_sent, bytes_recv;

    // Crear socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[-] Error creando socket");
        return 1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);

    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        perror("[-] Dirección inválida");
        close(sock);
        return 1;
    }

    // Conectar al servidor
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[-] Error conectando");
        close(sock);
        return 1;
    }

    printf("[+] Conectado al servidor %s:%d\n", server_ip, server_port);

    // Abrir archivo
    fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("[-] No se pudo abrir el archivo");
        close(sock);
        return 1;
    }

    // Enviar primer mensaje con "PUERTO SHIFT" para que el servidor lo sepa
    snprintf(buffer, sizeof(buffer), "%d %d", server_port, shift);
    if (send(sock, buffer, strlen(buffer), 0) < 0) {
        perror("[-] Error enviando info inicial");
        fclose(fp);
        close(sock);
        return 1;
    }    

    // Enviar archivo en bloques
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        bytes_sent = send(sock, buffer, bytes_read, 0);
        if (bytes_sent < 0) {
            perror("[-] Error enviando archivo");
            fclose(fp);
            close(sock);
            return 1;
        }
    }
    fclose(fp);

    // Indicar fin de envío
    shutdown(sock, SHUT_WR);

    // Recibir respuesta del servidor
    bytes_recv = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_recv <= 0) {
        printf("[-] No se recibió respuesta del servidor\n");
        close(sock);
        return 1;
    }

    buffer[bytes_recv] = '\0';
    printf("[+] Respuesta del servidor: %s\n", buffer);

    close(sock);
    return 0;
}