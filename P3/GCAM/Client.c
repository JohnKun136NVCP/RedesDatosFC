#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 4096

// Lee el contenido de un archivo y lo guarda en un buffer
void readFile(const char *filename, char *content, size_t size) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("[-] Error opening file");
        exit(EXIT_FAILURE);
    }
    size_t bytes = fread(content, 1, size - 1, fp);
    content[bytes] = '\0'; 
    fclose(fp);
}

// Conecta con un servidor, envía el mensaje y recibe la respuesta
void sendToServer(const char *server_ip, int server_port,
                  int target_port, int shift, const char *content) {
    int sockfd;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char message[BUFFER_SIZE];

    // Crear el socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("[-] Failed to create socket");
        return;
    }

    // Configuración de la dirección del servidor
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);

    // Conexión con el servidor
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[-] Failed to connect to server");
        close(sockfd);
        return;
    }

    // Construir mensaje en formato: PUERTO_OBJETIVO|DESPLAZAMIENTO|TEXTO
    snprintf(message, sizeof(message), "%d|%d|%s",
             target_port, shift, content);

    // Enviar mensaje al servidor
    send(sockfd, message, strlen(message), 0);

    // Recibir respuesta del servidor
    int bytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("[Client] Response from server %d: %s", server_port, buffer);
    } else {
        printf("[-] No response from server %d\n", server_port);
    }

    // Cerrar el socket
    close(sockfd);
}

// main
int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Uso: %s <IP_SERVIDOR> <PUERTO_OBJETIVO> <DESPLAZAMIENTO> <archivo.txt>\n", argv[0]);
        return 1;
    }

    // Argumentos de entrada
    const char *server_ip = argv[1];
    int target_port = atoi(argv[2]);
    int shift = atoi(argv[3]);
    const char *filename = argv[4];

    // Leer archivo de entrada
    char content[BUFFER_SIZE];
    readFile(filename, content, sizeof(content));

    // Lista de puertos de los servidores
    int server_ports[] = {49200, 49201, 49202};
    for (int i = 0; i < 3; i++) {
        sendToServer(server_ip, server_ports[i], target_port, shift, content);
    }

    return 0;
}


