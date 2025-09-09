#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Uso: %s <SERVIDOR_IP> <PUERTO> <DESPLAZAMIENTO> <Nombre del archivo>\n", argv[0]);
        return 1;
    }

    char *server_ip = argv[1];
    int port = atoi(argv[2]);
    int shift = atoi(argv[3]);
    char *filename = argv[4];

    // Leer el contenido del archivo
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("[-] Error al abrir el archivo");
        return 1;
    }

    char file_content[BUFFER_SIZE] = {0};
    fread(file_content, 1, sizeof(file_content) - 1, fp);
    fclose(fp);

    // Los puertos de los servidores
    int target_ports[] = {49200, 49201, 49202};
    int num_servers = 3;

    for (int i = 0; i < num_servers; i++) {
        int target_port = target_ports[i];
        int client_sock;
        struct sockaddr_in serv_addr;
        char buffer[BUFFER_SIZE] = {0};

        client_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (client_sock == -1) {
            perror("[-] Error al crear socket");
            continue;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(target_port);
        serv_addr.sin_addr.s_addr = inet_addr(server_ip);

        //printf("[+] Conectando al servidor en puerto %d...\n", target_port);
        if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            perror("[-] Error al conectar");
            close(client_sock);
            continue;
        }

        //printf("[+] Conectado al servidor en puerto %d\n", target_port);

        // Formar el mensaje: <PUERTO_OBJETIVO> <DESPLAZAMIENTO> <CONTENIDO_ARCHIVO>
        char mensaje[BUFFER_SIZE * 2];
        snprintf(mensaje, sizeof(mensaje), "%d %d %s", port, shift, file_content);

        // Enviar el mensaje al servidor
        if (send(client_sock, mensaje, strlen(mensaje), 0) < 0) {
            perror("[-] Error al enviar");
            close(client_sock);
            continue;
        }

       //printf("[+] Enviado a puerto %d: %s\n", target_port, mensaje);

        // Recibir respuesta del servidor
        int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            printf("[+] Respuesta del servidor (puerto %d): %s\n", target_port, buffer);
        } else {
            printf("[-] No se recibió respuesta del servidor (puerto %d)\n", target_port);
        }

        close(client_sock);
    }

    return 0;
}