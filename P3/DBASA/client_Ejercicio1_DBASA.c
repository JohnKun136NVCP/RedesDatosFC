#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 5){
        printf("Forma de uso: %s <SERVIDOR_IP> <PUERTO> <DESPLAZAMIENTO> <Nombre del archivo>\n", argv[0]);
        return 1;
    }
    char *server_ip = argv[1];
    char *port = argv[2];
    char *shift = argv[3];
    char *archivo= argv[4];
    char buffer[BUFFER_SIZE] = {0};
    char mensaje[BUFFER_SIZE];
    int client_sock;
    struct sockaddr_in serv_addr;

    FILE *fp = fopen(archivo, "r");
    if (fp == NULL) {
        perror("[-] Error al abrir el archivo");
        return 1;
    }

    // Obtener tamaño del archivo
    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    rewind(fp);

    // Reservar memoria
    char *contenido = malloc(filesize + 1);
    if (contenido == NULL) {
        perror("[-] Error de memoria");
        fclose(fp);
        return 1;
    }

    // Leer el archivo
    fread(contenido, 1, filesize, fp);
    contenido[filesize] = '\0';  // Null-terminator

    printf("[+] mensaje de '%s':\n%s\n", archivo, contenido);
    printf("###########################################################\n\n");
    const int puertos[] = {
        49200,
        49201,
        49202
    };

    for (int i = 0; i < 3; i++) {
        int puerto_actual = puertos[i];

        client_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (client_sock == -1) {
            perror("[-] Error al crear el socket");
            continue;
        }

        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(puerto_actual);
        serv_addr.sin_addr.s_addr = inet_addr(server_ip);

        if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            perror("[-] Error al conectarse al servidor");
            close(client_sock); 
            continue;          
        }

        printf("[+] Conectado al servidor en puerto %d\n", puerto_actual);

        snprintf(mensaje, sizeof(mensaje), "%s %s", port, shift);
        send(client_sock, mensaje, strlen(mensaje), 0);
        printf("[+][Client] puerto y shift enviados: %s %s\n", port, shift);

        int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            printf("[+][Client] Mensaje del Servidor: %s\n", buffer);

            if (strstr(buffer, "ACCESO ACEPTADO") != NULL) {
                printf("[+][Client] Autenticación correcta, enviando mensaje...\n");

                if (send(client_sock, contenido, strlen(contenido), 0) == -1) {
                    perror("[-] Error al enviar el contenido");
                } else {
                    printf("[+][Client] Contenido enviado al servidor\n");
                }

                send(client_sock, "\n", 1, 0);
            }
        } else {
            printf("[-] Server connection timeout\n");
        }
        printf("###########################################################\n\n");
        close(client_sock);
    }


    free(contenido);
    fclose(fp);
}