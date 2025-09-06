#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1024

void sendFile(const char *filename, int sockfd)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        perror("[-] Cannot open the file");
        return;
    }
    char buffer[BUFFER_SIZE];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0)
    {
        if (send(sockfd, buffer, bytes, 0) == -1)
        {
            perror("[-] Error to send the file");
            break;
        }
    }
    fclose(fp);
}

/* Función para configuración se socket y conexión al servidor
 */
int conectarServidor(struct sockaddr_in *serv_addr, int client_sock, const char *server_ip, int puerto)
{
    // Flechas porque son apuntadores
    serv_addr->sin_family = AF_INET;
    serv_addr->sin_port = htons(puerto);
    serv_addr->sin_addr.s_addr = inet_addr(server_ip);
    // intentamos conectarnos al socket de servidor usando nuestro socket de cliente con nuestro socket configurado para conexiones
    if (connect(client_sock, (struct sockaddr *)serv_addr, sizeof(*serv_addr)) < 0)
    {
        // Si el puerto está cerrado o no hay servidor avisamos
        printf("[*] CONNECTION TO SERVER %d failed\n", puerto);
        return 1;
    }
    return 0;
}

void autorizarEnviarArchivo(int client_sock, char *puerto, char *shift, char *rutaArchivo, int puertoServer)
{
    char mensaje[BUFFER_SIZE];
    char buffer[BUFFER_SIZE] = {0};

    // Formateamos y enviamos puerto objetivo y desplazamiento
    snprintf(mensaje, sizeof(mensaje), "%s %s", puerto, shift);
    send(client_sock, mensaje, strlen(mensaje), 0);

    // Esperamos la respuesta del servidor con recv que recibe datos de un socket ya conectado
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0)
    {
        buffer[bytes] = '\0';
        // Si la respuesta es positiva
        if (strstr(buffer, "REQUEST ACCEPTED") != NULL)
        {

            // Enviamos el nombre del archivo
            send(client_sock, rutaArchivo, strlen(rutaArchivo), 0);

            // Esperamos confirmación de recibido
            bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (bytes > 0 && (strstr(buffer, "FILENAME RECEIVED") != NULL))
            {

                // enviamos el archivo
                sendFile(rutaArchivo, client_sock);

                // indicamos que ya no enviaremos más datos
                shutdown(client_sock, SHUT_WR);

                // Esperamos confiramación de recibido y cifrado
                bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
                if (bytes > 0)
                {
                    buffer[bytes] = '\0';
                    printf("[*] SERVER RESPONSE %d: %s\n", puertoServer, buffer);
                }
                close(client_sock);
            }
        }
        else
        {
            printf("[*] SERVER RESPONSE %d: %s\n", puertoServer, buffer);
            close(client_sock);
        }
    }
    else
    {
        printf("[-] Server connection timeout\n");
    }
}

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        printf("Type: %s <server_ip> <port> <shift> <file_path>\n", argv[0]);
        return 1;
    }

    char *server_ip = argv[1];
    char *puerto = argv[2];
    char *shift = argv[3];
    char *rutaArchivo = argv[4];

    int client_sock;
    struct sockaddr_in serv_addr;

    int i;
    int puertos[3] = {49200, 49201, 49202};

    for (i = 0; i < 3; i++)
    {
        // Creamos el socket de tipo TCP ipv4
        client_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (client_sock == -1)
        {
            perror("[-] Error to create the socket");
            return 1;
        }
        if (conectarServidor(&serv_addr, client_sock, server_ip, puertos[i]) != 1)
        {
            autorizarEnviarArchivo(client_sock, puerto, shift, rutaArchivo, puertos[i]);
        }
    }

    close(client_sock);
    return 1;
}
