#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PUERTOS 3
int lista_puertos[PUERTOS] = {49200, 49201, 49202};

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "USO: %s <IP_SERVIDOR> <PUERTO_OBJETIVO> <DESPLAZAMIENTO> <archivo.txt>\n", argv[0]);
        return 1;
    }

    char *ip_servidor = argv[1];
    int puerto_obj = atoi(argv[2]);   // Puerto que debe procesar el servidor
    int desplazamiento = atoi(argv[3]);
    char *nombre_archivo = argv[4];

    // ======= Leer archivo completo en memoria =======
    FILE *fp = fopen(nombre_archivo, "r");
    if (!fp) {
        perror("No se pudo abrir el archivo");
        return 1;
    }
    char texto[4096];
    size_t len = fread(texto, 1, sizeof(texto) - 1, fp);
    fclose(fp);
    texto[len] = '\0';

    // ======= Intentar conexión con cada servidor =======
    for (int i = 0; i < PUERTOS; i++) {
        int puerto_actual = lista_puertos[i];
        int sockfd;
        struct sockaddr_in serv_addr;
        char buffer[4096];

        // Crear socket del cliente
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) { 
            perror("socket"); 
            continue; 
        }

        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(puerto_actual);
        serv_addr.sin_addr.s_addr = inet_addr(ip_servidor);

        // Conectarse al servidor
        if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            printf("[-]CLIENTE no pudo conectar al servidor en puerto %d\n", puerto_actual);
            close(sockfd);
            continue;
        }

        // ======= Armar mensaje con formato esperado =======
        // "<PUERTO_OBJETIVO> <DESPLAZAMIENTO> <TEXTO>"
        char mensaje[4500];
        snprintf(mensaje, sizeof(mensaje), "%d %d %s", puerto_obj, desplazamiento, texto);

        // Enviar al servidor
        send(sockfd, mensaje, strlen(mensaje), 0);

        // Recibir respuesta del servidor
        memset(buffer, 0, sizeof(buffer));
        int bytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            printf("[*]Respuesta del servidor (puerto %d): %s\n", puerto_actual, buffer);
        } else {
            printf("[-]CLIENTE: sin respuesta del servidor en puerto %d\n", puerto_actual);
        }

        close(sockfd); // cerramos conexión con este servidor
    }

    return 0;
}
