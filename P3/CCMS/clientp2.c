#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1024
//funcion para mandar la informacion del archivo de texto al servidor.
void send2Server(char *server_ip, int port, int puertoObjetivo, int shift, char *texto) {
    int client_sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];

    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock == -1) {
        perror("[-] Error al crear socket");
        return;
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);

    if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[-] Error al conectar");
        close(client_sock);
        return;
    }

    // Envia el formato al servidor con el puerto objetivo, el corrimiento y el texto a cifrar.
    char mensaje[BUFFER_SIZE];
    snprintf(mensaje, sizeof(mensaje), "%d %d %s", puertoObjetivo, shift, texto);
    printf("Enviando a %d: %s\n", port, mensaje);
    send(client_sock, mensaje, strlen(mensaje), 0);

    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("Respuesta del servidor %d: %s\n", port, buffer);
    }

    close(client_sock);
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Uso: %s <IP_SERVIDOR> <PUERTO_OBJETIVO> <CORRIMIENTO> <ARCHIVO>\n", argv[0]);
        return 1;
    }
    //Tipos de archivos que se esperan recibir
    char *server_ip = argv[1];
    int puertoObjetivo = atoi(argv[2]);
    int shift = atoi(argv[3]);
    char *archivo = argv[4];

    // Leer archivo
    FILE *fp = fopen(archivo, "r");
    if (!fp) {
        perror("[-] No se pudo abrir el archivo");
        return 1;
    }
    char texto[BUFFER_SIZE];
    fread(texto, 1, sizeof(texto) - 1, fp);
    fclose(fp);
    texto[sizeof(texto) - 1] = '\0';

    // Conexion a los 3 servidores
    send2Server(server_ip, 49200, puertoObjetivo, shift, texto);
    send2Server(server_ip, 49201, puertoObjetivo, shift, texto);
    send2Server(server_ip, 49202, puertoObjetivo, shift, texto);

    return 0;
}