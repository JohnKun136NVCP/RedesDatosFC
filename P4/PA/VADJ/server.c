#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 2048

int main(int argc, char *argv[]) {

    if (argc < 3) {
        fprintf(stderr, "Uso: %s <alias: s01|s02> <puerto>\n", argv[0]);
        return 1;
    }

    char *alias = argv[1];
    int port = atoi(argv[2]);
    char ip_addr[INET_ADDRSTRLEN];
    char save_dir[100];

    if (strcmp(alias, "s01") == 0) {
        strcpy(ip_addr, "192.168.100.101");
        snprintf(save_dir, sizeof(save_dir), "%s/s01", getenv("HOME"));
    } else if (strcmp(alias, "s02") == 0) {
        strcpy(ip_addr, "192.168.100.102");
        snprintf(save_dir, sizeof(save_dir), "%s/s02", getenv("HOME"));
    } else {
        fprintf(stderr, "[-] Alias no reconocido: %s. Use 's01' o 's02'.\n", alias);
        return 1;
    }

    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(client_addr);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("[-] Error al crear el socket");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip_addr);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "[-] Error en bind para %s:%d\n", ip_addr, port);
        return 1;
    }

    if (listen(server_sock, 5) < 0) {
        perror("[-] Error en listen");
        return 1;
    }

    printf("[+] Servidor para '%s' escuchando en %s:%d...\n", alias, ip_addr, port);

    while (1) {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
        if (client_sock < 0) {
            perror("[-] Error en accept");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("\n[+] Conexión aceptada desde %s\n", client_ip);

        char buffer[BUFFER_SIZE] = {0};
        int bytes_read = recv(client_sock, buffer, sizeof(buffer) - 1, 0);

        if (bytes_read > 0) {
            char *filename = strtok(buffer, "\n");
            char *file_content = filename + strlen(filename) + 1;

            if (filename && file_content) {
                char output_filepath[256];
                snprintf(output_filepath, sizeof(output_filepath), "%s/%s", save_dir, filename);

                FILE *fp = fopen(output_filepath, "w");
                if (fp == NULL) {
                    perror("[-] Error al crear el archivo de salida");
                    send(client_sock, "ERROR_ARCHIVO", strlen("ERROR_ARCHIVO"), 0);
                } else {
                    fprintf(fp, "%s", file_content);
                    fclose(fp);
                    printf("[+] Archivo guardado en '%s'\n", output_filepath);
                    send(client_sock, "PROCESADO", strlen("PROCESADO"), 0);
                }
            } else {
                 send(client_sock, "ERROR_FORMATO", strlen("ERROR_FORMATO"), 0);
            }
        } else {
            fprintf(stderr, "[-] Error al recibir datos del cliente.\n");
        }
        close(client_sock);
        printf("[+] Conexión con %s cerrada.\n", client_ip);
    }

    close(server_sock);
    return 0;
}
