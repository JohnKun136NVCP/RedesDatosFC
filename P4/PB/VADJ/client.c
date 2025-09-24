#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>

#define BUFFER_SIZE 2048

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <servidor_alias> <puerto> <archivo>\n", argv[0]);
        return 1;
    }

    char *server_alias = argv[1];
    int port = atoi(argv[2]);
    char *filename = argv[3];
    struct hostent *he = gethostbyname(server_alias);
    if (he == NULL) {
        herror("gethostbyname");
        fprintf(stderr, "[-] No se pudo resolver el host: %s\n", server_alias);
        return 1;
    }
    char *server_ip = inet_ntoa(*(struct in_addr *)he->h_addr);

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "[-] Error al abrir el archivo '%s'\n", filename);
        return 1;
    }
    char file_content[BUFFER_SIZE] = {0};
    fread(file_content, 1, sizeof(file_content) - 1, fp);
    fclose(fp);
    if (strlen(file_content) == 0) {
        printf("[-] El archivo '%s' está vacío o no se pudo leer\n", filename);
        return 1;
    }

    int client_sock;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};
    char mensaje[BUFFER_SIZE * 2];

    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock == -1) {
        perror("[-] Error al crear el socket");
        return 1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);

    if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[-] Error de conexión");
        close(client_sock);
        return 1;
    }

    printf("[+] Conectado al servidor %s (%s) en el puerto %d\n", server_alias, server_ip, port);
    snprintf(mensaje, sizeof(mensaje), "%s\n%s", filename, file_content);
    send(client_sock, mensaje, strlen(mensaje), 0);
    printf("[+] Archivo '%s' enviado.\n", filename);

    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("[+] Respuesta del servidor: %s\n", buffer);
    } else {
        printf("[-] No se recibió respuesta del servidor\n");
    }

    close(client_sock);
    return 0;
}
