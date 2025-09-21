//Client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 2048

int main(int argc, char *argv[]) {
    // Verificar que se hayan pasado los argumentos necesarios
    // argv[1] = IP del servidor
    // argv[2] = valor de shift para cifrado
    // argv[3] = nombre del archivo con el texto a enviar
    if (argc < 4) {
        printf("Uso: %s <IP_servidor> <shift> <archivo>\n", argv[0]);
        return 1;
    }

    char *ip_servidor = argv[1];      // IP del servidor
    int shift = atoi(argv[2]);         // Valor del shift convertido a entero

    // Abrir el archivo que contiene el texto
    FILE *fp = fopen(argv[3], "r");
    if (fp == NULL) {
        perror("Error al abrir el archivo");
        return 1;
    }

    // Leer el contenido del archivo en el buffer texto
    char texto[BUFFER_SIZE] = "";
    fread(texto, 1, sizeof(texto) - 1, fp);
    fclose(fp);

    // Definimos los tres puertos a los que nos conectaremos
    int puertos[] = {49200, 49201, 49202};

    // Bucle para conectar a cada servidor en cada puerto
    for (int i = 0; i < 3; i++) {
        int puerto_servidor = puertos[i];
        int puerto_objetivo = puerto_servidor; // En este caso, usamos el mismo puerto como objetivo

        int sockfd;
        struct sockaddr_in serv_addr;
        char buffer[BUFFER_SIZE];

        // Crear socket TCP
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("Error al crear socket");
            continue;   // Intentar con el siguiente puerto
        }

        // Configurar dirección del servidor
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(puerto_servidor);

        // Convertir la IP a formato binario
        if (inet_pton(AF_INET, ip_servidor, &serv_addr.sin_addr) <= 0) {
            perror("Dirección inválida");
            close(sockfd);
            continue;
        }

        // Conectarse al servidor en el puerto especificado
        if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            perror("Error al conectar");
            close(sockfd);
            continue;
        }

        // Formatear el mensaje para enviar: "<puerto_objetivo> <shift> <texto>"
        snprintf(buffer, sizeof(buffer), "%d %d %s", puerto_objetivo, shift, texto);

        // Enviar el mensaje al servidor
        send(sockfd, buffer, strlen(buffer), 0);

        // Recibir la respuesta del servidor
        int bytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytes > 0) {
            buffer[bytes] = '\0';  // Asegurar cadena terminada en null
            printf("Respuesta del servidor en puerto %d: %s\n", puerto_servidor, buffer);
        } else {
            printf("No se recibió respuesta del servidor en puerto %d\n", puerto_servidor);
        }

        // Cerrar el socket antes de conectar con el siguiente puerto
        close(sockfd);
    }

    return 0;
}
