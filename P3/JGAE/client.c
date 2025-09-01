#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT 7006
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Type: %s <key> <shift>\n", argv[0]);
        return 1;
    }

    char *clave = argv[1];
    char *shift = argv[2];
    int client_sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char mensaje[BUFFER_SIZE];
    char *server_ip = "192.168.0.112";

    // Creamos el socket de tipo TCP ipv4
    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock == -1) {
        perror("[-] Error to create the socket");
        return 1;
    }

    // configuramos el socket pero ahora para enviar datos
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    // aquí está la diferencia, ahora esta dirección IP es a dónde quiere enviar los datos, a quien se quiere conectar
    // inet_addr convierte de una cadena de texto con formato IPv4 a una dirección IP en formato binario
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);
    // intentamos conectarnos al socket de servidor usando nuestro socket de cliente con nuestro socket configurado para conexiones
    if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[-] Error to connect");
        close(client_sock);
        return 1;
    }

    printf("[+] Connected to server\n");

    // Después de la conexión hace un envío (handshake) de lado de cliente
    // Primero formateamos la cadena, para concatenar dos cadenas con un espacio en medio y lo guarda en mensaje
    snprintf(mensaje, sizeof(mensaje), "%s %s", clave, shift);
    // Enviamos el mensaje sin banderas, de forma predeterminada
    send(client_sock, mensaje, strlen(mensaje), 0);
    printf("[+][Client] Key and shift was sent: %s\n", mensaje);

    // Esperamos la respuesta del servidor con recv que recibe datos de un socket ya conectado
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        // el espacio que reservamos lo usamos para poner el caracter nulo
        buffer[bytes] = '\0';
        printf("[+][Client] Server message: %s\n", buffer);

        if (strstr(buffer, "ACCESS GRANTED") != NULL) {
            FILE *fp = fopen("info.txt", "w");
            if (fp == NULL) {
                perror("[-] Error to open the file");
                close(client_sock);
                return 1;
            }
            while ((bytes = recv(client_sock, buffer, sizeof(buffer), 0)) > 0) {
                fwrite(buffer, 1, bytes, fp);
            }
            printf("[+][Client] The file was save like 'info.txt'\n");
            fclose(fp);
        }
    } else {
        printf("[-] Server connection tiemeout\n");
    }

    close(client_sock);
    return 0;
}
