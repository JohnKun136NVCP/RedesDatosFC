#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>

#define BUFFER_SIZE 8192

// ======= Función de Cifrado César de practica 2 =======
void encryptCesar(char *text, int shift) {
    shift = ((shift % 26) + 26) % 26;

    for (int i = 0; text[i] != '\0'; i++) {
        unsigned char c = (unsigned char)text[i];

        if (isupper(c)) {
            // Mayúsculas
            text[i] = (char)(((c - 'A' + shift) % 26) + 'A');
        } else if (islower(c)) {
            // Minúsculas
            text[i] = (char)(((c - 'a' + shift) % 26) + 'a');
        } else {
            // Caracteres no alfabéticos quedan igual
            text[i] = (char)c;
        }
    }
}

// ======= Servidor TCP =======
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "USO: %s <PUERTO>\n", argv[0]);
        return 1;
    }

    int puerto = atoi(argv[1]);
    if (puerto <= 0 || puerto > 65535) {
        fprintf(stderr, "Puerto inválido: %s\n", argv[1]);
        return 1;
    }

    int sockfd = -1, newsockfd = -1;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;
    char buffer[BUFFER_SIZE];

    // Crear socket TCP
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); return 1; }

    // Reusar puerto si se reinicia rápido
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(puerto);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        close(sockfd);
        return 1;
    }

    if (listen(sockfd, 5) < 0) {
        perror("listen");
        close(sockfd);
        return 1;
    }

    printf("[*]SERVIDOR en puerto %d LISTO y ESCUCHANDO...\n", puerto);

    clilen = sizeof(cli_addr);
    while (1) {
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0) { perror("accept"); continue; }

        // Leer petición: "<PUERTO_OBJ> <SHIFT> <TEXTO>"
        memset(buffer, 0, sizeof(buffer));
        ssize_t n = read(newsockfd, buffer, sizeof(buffer) - 1);
        if (n <= 0) { close(newsockfd); continue; }

        int puerto_obj = 0, shift = 0;
        char texto[BUFFER_SIZE];
        texto[0] = '\0';

        if (sscanf(buffer, "%d %d %[^\n]", &puerto_obj, &shift, texto) < 2) {
            const char *errorFormato = "Formato inválido (esperado: <PUERTO> <DESPLAZAMIENTO> <TEXTO>)\n";
            write(newsockfd, errorFormato, strlen(errorFormato));
            close(newsockfd);
            continue;
        }

        if (puerto_obj != puerto) {
            // Rechazo
            printf("[*]SERVIDOR %d → Solicitud rechazada (cliente pidió puerto %d)\n",
                   puerto, puerto_obj);
            char rechazo[128];
            snprintf(rechazo, sizeof(rechazo), "Rechazado por servidor en puerto %d\n", puerto);
            write(newsockfd, rechazo, strlen(rechazo));
            close(newsockfd);
            continue;
        }

        // Aceptado
        printf("[*]SERVIDOR %d → Solicitud aceptada\n", puerto);
        encryptCesar(texto, shift);  // usamos tu función tal cual
        printf("[*]SERVIDOR %d → Archivo recibido y cifrado: %s\n", puerto, texto);

        char respuesta[BUFFER_SIZE];
        // Limito el %s a 4000 caracteres para evitar warning de truncamiento
        snprintf(respuesta, sizeof(respuesta),
                 "Procesado por servidor en puerto %d: %.4000s\n", puerto, texto);
        write(newsockfd, respuesta, strlen(respuesta));

        close(newsockfd);
    }

    close(sockfd);
    return 0;
}
