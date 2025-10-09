// ClientP5
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUFSZ 4096

int main(int argc, char *argv[]) {
    // Validar argumentos
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <IP/Alias_Servidor> <Puerto> <Archivo_a_Enviar>\n", argv[0]);
        return 1;
    }

    const char *server_host = argv[1];
    int port = atoi(argv[2]);
    const char *filepath = argv[3];

    // Puro manejo de errores
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        perror("Error abriendo el archivo");
        return 1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
      perror("socket"); fclose(fp);
      return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    // Resolver el hostname a una dirección ip
    struct hostent *host;
    host = gethostbyname(server_host);
    
    if (host == NULL) {
        fprintf(stderr, "Error: No se pudo resolver el hostname '%s'\n", server_host);
        fclose(fp);
        close(sock);
        return 1;
    }
    // Copiamos la dirección ip resuelta a nuestra estructura de dirección
    memcpy(&server_addr.sin_addr, host->h_addr_list[0], host->h_length);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "Fallo en la conexión a %s (%s)\n", server_host, inet_ntoa(server_addr.sin_addr));
        perror("connect");
        fclose(fp);
        close(sock);
        return 1;
    }

    printf("[CLIENTE] Conectado a %s:%d. Enviando archivo: %s\n", server_host, port, filepath);

    char buffer[BUFSZ];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, BUFSZ, fp)) > 0) {
        if (send(sock, buffer, bytes_read, 0) < 0) {
            perror("send");
            break;
        }
    }

    fclose(fp);
    shutdown(sock, SHUT_WR);
    printf("[CLIENTE] Archivo enviado. Esperando respuesta...\n");
    while ((bytes_read = recv(sock, buffer, BUFSZ - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';
        printf("%s", buffer);
    }

    close(sock);
    return 0;
}

