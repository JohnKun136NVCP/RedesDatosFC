#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <time.h>

#define PORT 49200
#define BUFFER_SIZE 8192

// Función auxiliar para registrar mensajes con fecha y hora
void log_message(const char *msg) {
    time_t now = time(NULL);
    char tbuf[64];
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    printf("[%s] %s\n", tbuf, msg);
}

// Guardar archivo recibido en la carpeta del alias
int save_file(const char *alias, const char *filename, const char *data) {
    char path[512];
    char *home = getenv("HOME");
    snprintf(path, sizeof(path), "%s/%s/%s", home, alias, filename);

    FILE *fp = fopen(path, "w");
    if (!fp) return -1;

    fputs(data, fp);
    fclose(fp);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <alias>\n", argv[0]);
        return 1;
    }

    char *alias = argv[1];
    int server_fd, client_fd;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t cli_len = sizeof(cli_addr);

    // Crear socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error creando socket");
        return 1;
    }

    // Resolver alias a IP
    struct hostent *he = gethostbyname(alias);
    if (!he) {
        perror("Error resolviendo alias");
        close(server_fd);
        return 1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr = *((struct in_addr *)he->h_addr);

    // Asociar socket
    if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Error en bind");
        close(server_fd);
        return 1;
    }

    // Escuchar
    if (listen(server_fd, 5) < 0) {
        perror("Error en listen");
        close(server_fd);
        return 1;
    }

    log_message("Servidor iniciado, esperando conexiones...");

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&cli_addr, &cli_len);
        if (client_fd < 0) {
            perror("Error en accept");
            continue;
        }

        log_message("Cliente conectado.");

        // Estado inicial
        send(client_fd, "Servidor listo para recibir", 27, 0);

        // Recibir nombre del archivo
        char fname[256] = {0};
        int nbytes = recv(client_fd, fname, sizeof(fname)-1, 0);
        if (nbytes <= 0) {
            log_message("Error al recibir nombre de archivo");
            close(client_fd);
            continue;
        }
        fname[nbytes] = '\0';
        send(client_fd, "Nombre de archivo recibido", 26, 0);

        // Recibir contenido del archivo
        char buffer[BUFFER_SIZE] = {0};
        nbytes = recv(client_fd, buffer, sizeof(buffer)-1, 0);
        if (nbytes <= 0) {
            log_message("Error al recibir contenido");
            close(client_fd);
            continue;
        }
        buffer[nbytes] = '\0';
        send(client_fd, "Archivo en procesamiento", 24, 0);

        // Guardar archivo
        if (save_file(alias, fname, buffer) == 0) {
            log_message("Archivo guardado correctamente.");
            send(client_fd, "Archivo guardado", 17, 0);
        } else {
            perror("Error guardando archivo");
            send(client_fd, "Error guardando archivo", 24, 0);
        }

        close(client_fd);
    }

    close(server_fd);
    return 0;
}
