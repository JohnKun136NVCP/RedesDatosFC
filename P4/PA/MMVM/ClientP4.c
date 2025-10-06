// ClientP4.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFSZ 4096

int main(int argc, char *argv[]) {
    // Ver que si recibimos todos los paremetros.
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <IP/Alias_Servidor> <Puerto> <Archivo_a_Enviar>\n", argv[0]);
        return 1;
    }

    const char *server_host = argv[1];
    int port = atoi(argv[2]);
    const char *filepath = argv[3];

    // Abrir el archivos a mandar
    FILE *fp = fopen(filepath, "rb");

    if (!fp) {
        perror("Error abriendo el archivo");
        return 1;
    }

    // Hacemos un socket para la comunicación
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); fclose(fp); return 1; }

    // Preparar dirección del servidor
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    // Convertir el string de ip a formato de red
    inet_pton(AF_INET, server_host, &server_addr.sin_addr;

    // Inicia la conexión
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        fclose(fp);
        close(sock);
        return 1;
    }

    printf("[CLIENTE] Conectado a %s:%d. Enviando archivo: %s\n", server_host, port, filepath);

    char buffer[BUFSZ];
    size_t bytes_read;

    // Mientras podamos leer el archivo, enviamos al servidor
    while ((bytes_read = fread(buffer, 1, BUFSZ, fp)) > 0) {
        if (send(sock, buffer, bytes_read, 0) < 0) {
            perror("send");
            break;
        }
    }

    // Cerramos el archivo
    fclose(fp);

    // Avisamos al servidor que tya no enviaremos mas datos
    shutdown(sock, SHUT_WR);
    // Mostrar la respuesta del servidor
    printf("[CLIENTE] Archivo enviado. Esperando respuesta...\n");
    while ((bytes_read = recv(sock, buffer, BUFSZ - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';
        printf("%s", buffer);
    }

    // Cerramos el socket y damos fin al caos :)
    close(sock);
    return 0;
}

