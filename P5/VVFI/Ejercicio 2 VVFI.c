// client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUFFER_SIZE 2048

void enviarArchivo(const char *host_servidor, int puerto, int shift, const char *archivo) {
    FILE *fp = fopen(archivo, "r");
    if (!fp) {
        perror("Error al abrir archivo");
        return;
    }

    char texto[BUFFER_SIZE] = "";
    fread(texto, 1, sizeof(texto) - 1, fp);
    fclose(fp);

    int sockfd;
    struct sockaddr_in serv_addr;
    struct hostent *he;
    char buffer[BUFFER_SIZE];

    if ((he = gethostbyname(host_servidor)) == NULL) {
        perror("Error en gethostbyname");
        return;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error al crear socket");
        return;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(puerto);
    memcpy(&serv_addr.sin_addr, he->h_addr_list[0], he->h_length);

    printf("[Cliente] Conectando a servidor %s:%d...\n", host_servidor, puerto);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Error al conectar");
        close(sockfd);
        return;
    }

    snprintf(buffer, sizeof(buffer), "%d %s", shift, texto);
    send(sockfd, buffer, strlen(buffer), 0);

    int bytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("Respuesta → %s\n", buffer);
    }

    close(sockfd);
}

int main(int argc, char *argv[]) {
    if (argc < 6) {
        printf("Uso: %s <alias_origen> <puerto> <shift> <archivo> <servidor1> [servidor2 ...]\n", argv[0]);
        return 1;
    }

    char *alias_origen = argv[1];
    int puerto = atoi(argv[2]);
    int shift = atoi(argv[3]);
    char *archivo = argv[4];

    printf("[+] Cliente %s enviando %s con shift=%d al puerto %d\n", alias_origen, archivo, shift, puerto);

    for (int i = 5; i < argc; i++) {
        enviarArchivo(argv[i], puerto, shift, archivo);
    }

    return 0;
}
