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
    char message[BUFFER_SIZE * 2];

    // Crear el socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        return; // Silencioso en errores
    }

    // Configuración de la dirección del servidor
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);

    // Conexión con el servidor
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sockfd);
        return;
    }

    // Construir mensaje en formato: PUERTO_OBJETIVO|DESPLAZAMIENTO|TEXTO
    snprintf(message, sizeof(message), "%d|%d|%s", target_port, shift, content);

    // Enviar mensaje al servidor
    send(sockfd, message, strlen(message), 0);

    // Recibir respuesta del servidor
    recv(sockfd, buffer, sizeof(buffer) - 1, 0);

    // Cerrar el socket
    close(sockfd);
}

// main
int main(int argc, char *argv[]) {
    if (argc != 9) {
        fprintf(stderr, "Uso: %s <IP_SERVIDOR> <PUERTO1> <PUERTO2> <PUERTO3> <ARCHIVO1> <ARCHIVO2> <ARCHIVO3> <DESPLAZAMIENTO>\n", argv[0]);
        return 1;
    }

    // Argumentos de entrada
    const char *server_ip = argv[1];
    int port1 = atoi(argv[2]);
    int port2 = atoi(argv[3]);
    int port3 = atoi(argv[4]);
    const char *file1 = argv[5];
    const char *file2 = argv[6];
    const char *file3 = argv[7];
    int shift = atoi(argv[8]);

    // Lista de puertos de los servidores
    int server_ports[] = {49200, 49201, 49202};
    
    // Enviar primer archivo al primer puerto objetivo
    char content1[BUFFER_SIZE];
    readFile(file1, content1, sizeof(content1));
    for (int j = 0; j < 3; j++) {
        sendToServer(server_ip, server_ports[j], port1, shift, content1);
    }
    printf("[Client] Puerto %d: Archivo cifrado recibido\n", port1);

    // Enviar segundo archivo al segundo puerto objetivo
    char content2[BUFFER_SIZE];
    readFile(file2, content2, sizeof(content2));
    for (int j = 0; j < 3; j++) {
        sendToServer(server_ip, server_ports[j], port2, shift, content2);
    }
    printf("[Client] Puerto %d: Archivo cifrado recibido\n", port2);

    // Enviar tercer archivo al tercer puerto objetivo
    char content3[BUFFER_SIZE];
    readFile(file3, content3, sizeof(content3));
    for (int j = 0; j < 3; j++) {
        sendToServer(server_ip, server_ports[j], port3, shift, content3);
    }
    printf("[Client] Puerto %d: Archivo cifrado recibido\n", port3);

    return 0;
}
