#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

void enviarArchivo(char *ip, int puerto, int desplazamiento, char *nombreArchivo) {
    int sock;
    struct sockaddr_in servidor;
    FILE *archivo;
    char buffer[1024], respuesta[1024];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Error en socket");
        exit(1);
    }

    servidor.sin_family = AF_INET;
    servidor.sin_port = htons(puerto);
    inet_pton(AF_INET, ip, &servidor.sin_addr);

    if (connect(sock, (struct sockaddr *)&servidor, sizeof(servidor)) < 0) {
        perror("Error en conexión");
        close(sock);
        return;
    }

    archivo = fopen(nombreArchivo, "r");
    if (!archivo) {
        perror("Error abriendo archivo");
        close(sock);
        return;
    }

    fread(buffer, sizeof(char), sizeof(buffer), archivo);
    fclose(archivo);

    send(sock, &puerto, sizeof(int), 0);
    send(sock, &desplazamiento, sizeof(int), 0);
    send(sock, buffer, sizeof(buffer), 0);

    recv(sock, respuesta, sizeof(respuesta), 0);
    printf("Respuesta del servidor en puerto %d: %s\n", puerto, respuesta);

    close(sock);
}

int main(int argc, char *argv[]) {
    if (argc < 5 || (argc - 3) % 2 != 0) {
        printf("Uso: %s <IP> <DESPLAZAMIENTO> <archivo1> <puerto1> [<archivo2> <puerto2> ...]\n", argv[0]);
        return 1;
    }

    char *ip = argv[1];
    int desplazamiento = atoi(argv[2]);

    for (int i = 3; i < argc; i += 2) {
        char *archivo = argv[i];
        int puerto = atoi(argv[i + 1]);
        enviarArchivo(ip, puerto, desplazamiento, archivo);
    }

    return 0;
}
