#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>

#define MAIN_PORT 49200
#define BUFFER_SIZE 1024

void send_file(int client_sock, const char *filename)
{
    char buffer[BUFFER_SIZE];

    // Enviar el nombre del archivo
    if (send(client_sock, filename, strlen(filename), 0) < 0)
    {
        perror("[-] Error al enviar el nombre del archivo");
        return;
    }
    sleep(1);

    // Abrir el archivo
    FILE *fp = fopen(filename, "rb");
    if (!fp)
    {
        perror("[-] Error al abrir el archivo");
        return;
    }

    // Leer y enviar el contenido del archivo
    int bytes;
    while ((bytes = fread(buffer, 1, BUFFER_SIZE, fp)) > 0)
    {
        if (send(client_sock, buffer, bytes, 0) < 0)
        {
            perror("[-] Error al enviar datos");
            fclose(fp);
            return;
        }
        usleep(100000);
    }
    fclose(fp);

    // Enviar fin del archivo
    if (send(client_sock, "EOF", 3, 0) < 0)
    {
        perror("[-] Error al enviar EOF");
        return;
    }
    printf("[+] Archivo %s enviado\n", filename);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("USO: <ALIAS> <ARCHIVO_1> <ARCHIVO_2> ...\n");
        return 1;
    }

    char *alias = argv[1];
    struct addrinfo hints, *res;
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", MAIN_PORT);

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // Obtener dirección IP del alias
    if (getaddrinfo(alias, port_str, &hints, &res) != 0)
    {
        perror("[-] Error al resolver alias");
        return 1;
    }

    // Crear socket
    int client_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (client_sock < 0)
    {
        perror("[-] Error al crear el socket");
        return 1;
    }

    if (connect(client_sock, res->ai_addr, res->ai_addrlen) < 0)
    {
        perror("[-] Error al conectar al servidor");
        exit(1);
    }

    printf("[+] Conectado al servidor en el puerto %d\n", MAIN_PORT);

    // Enviar cada archivo
    for (int i = 2; i < argc; i++)
    {
        send_file(client_sock, argv[i]);
    }

    close(client_sock);
    freeaddrinfo(res); 
    printf("[+] Conexión cerrada\n");
    return 0;
}
