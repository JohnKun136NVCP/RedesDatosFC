#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define PORT 7006
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    // Se espera que el cliente reciba exactamente 2 argumentos (clave y desplazamiento)
    if (argc != 3) {
        printf("Uso: %s <key> <shift>\n", argv[0]);
        return 1;
    } else {
        printf("[+] Cliente en ejecuciÃ³n...\n");
        printf("[+] Clave ingresada: %s\n", argv[1]);
        printf("[+] Desplazamiento: %s\n", argv[2]);
        printf("[+] Preparado para enviar la informaciÃ³n al servidor\n");
    }

    char *clave = argv[1];
    char *shift = argv[2];
    int client_sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char mensaje[BUFFER_SIZE];
    char *server_ip = "192.168.100.50"; // DirecciÃ³n IP del servidor a conectar

    // CreaciÃ³n del socket TCP
    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock == -1) {
        perror("[-] FallÃ³ la creaciÃ³n del socket");
        return 1;
    } else {
        printf("[+] Socket generado correctamente\n");
    }

    // ConfiguraciÃ³n de la estructura de conexiÃ³n
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);

    // Intentar establecer comunicaciÃ³n con el servidor
    if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[-] No se pudo establecer la conexiÃ³n");
        printf("errno: %d (%s)\n", errno, strerror(errno));
        close(client_sock);
        return 1;
    }

    printf("[+] ConexiÃ³n establecida con el servidor\n");

    // ConstrucciÃ³n del mensaje a enviar: clave + shift
    snprintf(mensaje, sizeof(mensaje), "%s %s", clave, shift);
    send(client_sock, mensaje, strlen(mensaje), 0);
    printf("[+][Cliente] Se enviÃ³ clave y desplazamiento: %s\n", mensaje);

    // Esperar respuesta inicial del servidor
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);

    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("[+][Cliente] Respuesta del servidor: %s\n", buffer);

        // Si el servidor concede acceso, se procede a guardar la informaciÃ³n en un archivo
        if (strstr(buffer, "ACCESS GRANTED") != NULL) {
            FILE *fp = fopen("info.txt", "w");
            if (fp == NULL) {
                perror("[-] No fue posible abrir el archivo");
                close(client_sock);
                return 1;
            }
            
            // Recibir el contenido y escribirlo en "info.txt"
            while ((bytes = recv(client_sock, buffer, sizeof(buffer), 0)) > 0) {
                fwrite(buffer, 1, bytes, fp);
            }
            
            printf("[+][Cliente] Archivo recibido y guardado como 'info.txt'\n");
            fclose(fp);
        }
    } else {
        printf("[-] No se obtuvo respuesta (timeout)\n");
    }

    // Finalizar comunicaciÃ³n
    close(client_sock);
    return 0;
}
