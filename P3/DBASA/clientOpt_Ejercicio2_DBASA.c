#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc < 5 || ((argc - 2) % 3 != 0)) {
        printf("Forma de uso: %s <SERVIDOR_IP> <PUERTO1> <DESPLAZAMIENTO1> <ARCHIVO1> [<PUERTO2> <DESPLAZAMIENTO2> <ARCHIVO2> ...]\n", argv[0]);
        return 1;
    }

    char *server_ip = argv[1];
    int num_conjuntos = (argc - 2) / 3;

    for (int set = 0; set < num_conjuntos; set++) {
        char *port_str   = argv[2 + set * 3];
        char *shift      = argv[3 + set * 3];
        char *archivo    = argv[4 + set * 3];
        int puerto       = atoi(port_str);

        printf("========== Procesando conjunto %d ==========\n", set + 1);
        printf("Puerto: %d | Desplazamiento: %s | Archivo: %s\n", puerto, shift, archivo);

        FILE *fp = fopen(archivo, "r");
        if (fp == NULL) {
            perror("[-] Error al abrir el archivo");
            continue;
        }

        fseek(fp, 0, SEEK_END);
        long filesize = ftell(fp);
        rewind(fp);

        char *contenido = malloc(filesize + 1);
        if (contenido == NULL) {
            perror("[-] Error de memoria");
            fclose(fp);
            continue;
        }

        fread(contenido, 1, filesize, fp);
        contenido[filesize] = '\0';
        fclose(fp);

        int client_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (client_sock == -1) {
            perror("[-] Error al crear el socket");
            free(contenido);
            continue;
        }

        struct sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(puerto);
        serv_addr.sin_addr.s_addr = inet_addr(server_ip);

        if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            perror("[-] Error al conectarse al servidor");
            close(client_sock);
            free(contenido);
            continue;
        }

        printf("[+] Conectado al servidor en puerto %d\n", puerto);

        char mensaje[BUFFER_SIZE];
        snprintf(mensaje, sizeof(mensaje), "%s %s", port_str, shift);
        send(client_sock, mensaje, strlen(mensaje), 0);
        printf("[Client] puerto y shift enviados: %s %s\n", port_str, shift);

        char buffer[BUFFER_SIZE] = {0};
        int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            printf("[Server] Respuesta: %s\n", buffer);

            if (strstr(buffer, "ACCESO ACEPTADO") != NULL) {
                printf("[Client] Autenticación correcta, enviando archivo...\n");
                if (send(client_sock, contenido, strlen(contenido), 0) == -1) {
                    perror("[-] Error al enviar el contenido");
                } else {
                    printf("[Client] Contenido de '%s' enviado.\n", archivo);
                }
                send(client_sock, "\n", 1, 0);  // delimitador
            }
        } else {
            printf("[-] Server connection timeout\n");
        }

        close(client_sock);
        free(contenido);
        printf("============================================\n\n");
    }

    return 0;
}
