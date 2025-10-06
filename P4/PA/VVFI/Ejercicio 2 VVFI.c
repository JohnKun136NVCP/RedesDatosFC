//Client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>   // se agregó para gethostbyname

#define BUFFER_SIZE 2048

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Uso: %s <host_servidor> <shift> <archivo>\n", argv[0]);
        return 1;
    }

    char *host_servidor = argv[1];   // puede ser alias o IP
    int shift = atoi(argv[2]);

    FILE *fp = fopen(argv[3], "r");
    if (fp == NULL) {
        perror("Error al abrir el archivo");
        return 1;
    }

    char texto[BUFFER_SIZE] = "";
    fread(texto, 1, sizeof(texto) - 1, fp);
    fclose(fp);

    int puertos[] = {49200, 49201, 49202};

    for (int i = 0; i < 3; i++) {
        int puerto_servidor = puertos[i];
        int puerto_objetivo = puerto_servidor;

        int sockfd;
        struct sockaddr_in serv_addr;
        char buffer[BUFFER_SIZE];

        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("Error al crear socket");
            continue;
        }

        memset(&serv_addr, 0, sizeof(serv_addr)); // se agregó
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(puerto_servidor);

        // se cambió inet_pton por gethostbyname
        struct hostent *he;
        if ((he = gethostbyname(host_servidor)) == NULL) {
            perror("Error en gethostbyname");
            close(sockfd);
            continue;
        }
        memcpy(&serv_addr.sin_addr, he->h_addr_list[0], he->h_length);

        if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            perror("Error al conectar");
            close(sockfd);
            continue;
        }

        snprintf(buffer, sizeof(buffer), "%d %d %s", puerto_objetivo, shift, texto);
        send(sockfd, buffer, strlen(buffer), 0);

        int bytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            printf("Respuesta del servidor en puerto %d: %s\n", puerto_servidor, buffer);
        } else {
            printf("No se recibió respuesta en puerto %d\n", puerto_servidor);
        }

        close(sockfd);
    }

    return 0;
}
