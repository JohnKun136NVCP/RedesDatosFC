#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1024
int MAIN_PORT = 49200;

int main(int argc, char *argv[])
{
    int client_sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    int bytes; 

    if (argc != 4)
    {
        printf("Uso: <IP del servidor> <Puerto a solicitar> <Archivo>\n");
        return 1;
    }

    char *server_ip = argv[1];
    int port = atoi(argv[2]);
    char *filename = argv[3];

    if ((client_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[-] Error al crear el socket");
        return 1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // Convertir dirección IP
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0)
    {
        perror("[-] Dirección inválida/ Dirección no soportada");
        close(client_sock);
        return 1;
    }

    // Conectar al servidor
    if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("[-] Error al conectar al servidor");
        close(client_sock);
        return 1;
    }
    printf("[+] Conectado al servidor\n");

    // Recibir puerto dinámico
    int net_port;
    if (recv(client_sock, &net_port, sizeof(net_port), 0) <= 0)
    {
        printf("[-] No se recibio el puerto dinámico\n");
        close(client_sock);
        return 1;
    }
    int dynamic_port = ntohl(net_port);
    printf("[+] Puerto dinámico recibido: %d\n", dynamic_port);
    close(client_sock);

    // Conectar al puerto dinámico
    if ((client_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[-] Error al crear el socket");
        return 1;
    }
    serv_addr.sin_port = htons(dynamic_port);

    // Conectar al servidor en el puerto dinámico
    if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("[-] Error al conectar al servidor en el puerto dinámico");
        close(client_sock);
        return 1;
    }
    printf("[+] Conectado al servidor en el puerto dinámico %d\n", dynamic_port);

    // Enviar nombre del archivo
    if (send(client_sock, filename, strlen(filename), 0) == -1)
    {
        perror("[-] Error al enviar el nombre del archivo");
        close(client_sock);
        return 1;
    }

    // Enviar contenido del archivo
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("[-] Error al abrir archivo");
        close(client_sock);
        return 1;
    }

    while ((bytes = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        send(sock, buffer, bytes, 0);
    }
    fclose(fp);
    shutdown(client_sock, SHUT_WR);


    // Recibir estado
    bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0)
    {
        printf("[-] No se recibio respuesta del servidor\n");
        close(client_sock);
        return 1;
    }

    buffer[bytes] = '\0';
    printf("[+][Client] Mensaje del servidor: %s\n", buffer);
    if (strstr(buffer, "ACCESO DENEGADO") != NULL)
    {
        printf("[-] Acceso denegado por el servidor\n");
        close(client_sock);
        return 1;
    }

    close(client_sock);
    return 0;
}
