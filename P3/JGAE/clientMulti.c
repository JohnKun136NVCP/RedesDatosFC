#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT 7006
#define BUFFER_SIZE 1024


void sendFile(const char *filename, int sockfd) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("[-] Cannot open the file");
        return;
    }
    char buffer[BUFFER_SIZE];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if (send(sockfd, buffer, bytes, 0) == -1) {
            perror("[-] Error to send the file");
            break;
        }
    }
    fclose(fp);
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Type: %s <server_ip> <port> <shift> <file_path>\n", argv[0]);
        return 1;
    }

    char *server_ip = argv[1];
    char *puerto = argv[2];
    char *shift= argv[3];
    char *rutaArchivo = argv[4];

    int client_sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char mensaje[BUFFER_SIZE];
    //char *server_ip = "192.168.0.112";

    // Creamos el socket de tipo TCP ipv4
    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock == -1) {
        perror("[-] Error to create the socket");
        return 1;
    }

    // configuramos el socket pero ahora para enviar datos
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(puerto));
    // aquí está la diferencia, ahora esta dirección IP es a dónde quiere enviar los datos, a quien se quiere conectar
    // inet_addr convierte de una cadena de texto con formato IPv4 a una dirección IP en formato binario
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);
    // intentamos conectarnos al socket de servidor usando nuestro socket de cliente con nuestro socket configurado para conexiones
    if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr))  < 0) {
        perror("[-] Error to connect");
        close(client_sock);
        return 1;
    }

    printf("[+] Connected to server\n");
    // Formateamos y enviamos puerto objetivo y desplazamiento
    snprintf(mensaje, sizeof(mensaje), "%s %s", puerto, shift);
    send(client_sock, mensaje, strlen(mensaje), 0);
    printf("[+][Client] Port and shift was sent: %s\n", mensaje);

    // Esperamos la respuesta del servidor con recv que recibe datos de un socket ya conectado
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        // Si la respuesta es positiva
        if (strstr(buffer, "REQUEST ACCEPTED") != NULL) {
            printf("[+][Client] Server response: %s\n", buffer);
            printf("[+][Client] Sending file %s\n", rutaArchivo);
            
            // Enviamos el nombre del archivo
            send(client_sock,rutaArchivo, strlen(rutaArchivo), 0);

            // Damos un tiempo
            sleep(1);

            // enviamos el archivo
            sendFile(rutaArchivo, client_sock);
        }
        
    }else {
        printf("[-] Server connection timeout\n");
    }
    
    close(client_sock);
    return 0;
}
