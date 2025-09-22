#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>

#define BUFFER_SIZE 1024

void log_event(const char *server, const char *filename, const char *status) {
    time_t now = time(NULL);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    FILE *log_file = fopen("client_log.txt", "a");
    if (log_file) {
        fprintf(log_file, "%s - %s - %s - %s\n", time_str, server, filename, status);
        fclose(log_file);
    }
}

void send_file(int socket, const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("fopen");
        log_event("N/A", filename, "ERROR_AL_ABRIR_ARCHIVO");
        exit(1);
    }

    // Obtener solo el nombre del archivo (sin ruta)
    const char *basename = strrchr(filename, '/');
    if (basename) {
        basename++;
    } else {
        basename = filename;
    }

    // Enviar longitud del nombre
    int filename_len = strlen(basename);
    if (send(socket, &filename_len, sizeof(filename_len), 0) < 0) {
        perror("send filename_len");
        fclose(file);
        log_event("N/A", filename, "ERROR_AL_ENVIAR_NOMBRE");
        exit(1);
    }

    // Enviar nombre del archivo
    if (send(socket, basename, filename_len, 0) < 0) {
        perror("send filename");
        fclose(file);
        log_event("N/A", filename, "ERROR_AL_ENVIAR_NOMBRE");
        exit(1);
    }

    // Enviar contenido del archivo
    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        if (send(socket, buffer, bytes_read, 0) < 0) {
            perror("send file content");
            break;
        }
    }

    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <host> <puerto> <archivo>\n", argv[0]);
        exit(1);
    }

    char *server = argv[1];
    int port = atoi(argv[2]);
    char *filename = argv[3];

    struct hostent *he;
    struct sockaddr_in server_addr;
    int sockfd;

    if ((he = gethostbyname(server)) == NULL) {
        perror("gethostbyname");
        log_event(server, filename, "ERROR_RESOLUCION_HOST");
        exit(1);
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        log_event(server, filename, "ERROR_CREACION_SOCKET");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr = *((struct in_addr *)he->h_addr);
    memset(&(server_addr.sin_zero), '\0', 8);

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
        perror("connect");
        log_event(server, filename, "ERROR_CONEXION");
        exit(1);
    }

    printf("Conectado a %s:%d, enviando %s\n", server, port, filename);
    log_event(server, filename, "ENVIO_INICIADO");
    
    send_file(sockfd, filename);
    close(sockfd);
    
    log_event(server, filename, "ENVIO_COMPLETADO");
    printf("Archivo %s enviado exitosamente a %s:%d\n", filename, server, port);

    return 0;
}


