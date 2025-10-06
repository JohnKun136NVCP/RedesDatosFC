#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>

#define BUFFER_SIZE 1024  // Tamaño del buffer para enviar datos

// Función para registrar eventos en un archivo de log
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

// Envía un archivo al servidor a través de un socket
void send_file(int socket, const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        log_event("N/A", filename, "ERROR_AL_ABRIR_ARCHIVO");
        exit(1);
    }

    // Extraer solo el nombre base del archivo (sin ruta)
    const char *basename = strrchr(filename, '/');
    if (basename) basename++;
    else basename = filename;

    // Enviar la longitud y el nombre del archivo
    int filename_len = strlen(basename);
    if (send(socket, &filename_len, sizeof(filename_len), 0) < 0) {
        fclose(file);
        exit(1);
    }
    if (send(socket, basename, filename_len, 0) < 0) {
        fclose(file);
        exit(1);
    }

    // Enviar el contenido del archivo por bloques
    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    size_t total_sent = 0;
    
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        ssize_t sent = send(socket, buffer, bytes_read, 0);
        if (sent < 0) break;
        total_sent += sent;
    }

    fclose(file);
    log_event("Server", filename, "ENVIADO");
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <host> <puerto> <archivo>\n", argv[0]);
        exit(1);
    }

    char *server = argv[1];
    int port = atoi(argv[2]);
    char *filename = argv[3];


    // Verificación de estado

    if (strcmp(filename, "status") == 0) {
        struct hostent *he;
        struct sockaddr_in server_addr;
        int sockfd;

        // Resolver host
        if ((he = gethostbyname(server)) == NULL) {
            printf("[%s] Inactivo\n", server);
            exit(1);
        }

        // Crear socket
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            printf("[%s] Inactivo\n", server);
            exit(1);
        }

        // Configurar dirección del servidor
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        server_addr.sin_addr = *((struct in_addr *)he->h_addr);
        memset(&(server_addr.sin_zero), '\0', 8);

        // Definir tiempo de espera (2 segundos)
        struct timeval timeout = {2, 0};
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        // Intentar conexión
        if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
            printf("[%s] Inactivo\n", server);
            close(sockfd);
            exit(1);
        }

        printf("[%s] Activo\n", server);
        close(sockfd);
        return 0;
    }

    // Envio de archivo
    struct hostent *he;
    struct sockaddr_in server_addr;
    int sockfd;

    // Resolver host
    if ((he = gethostbyname(server)) == NULL) {
        printf("Error: No se pudo resolver %s\n", server);
        exit(1);
    }

    // Crear socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("Error: No se pudo crear socket\n");
        exit(1);
    }

    // Configurar dirección del servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr = *((struct in_addr *)he->h_addr);
    memset(&(server_addr.sin_zero), '\0', 8);

    // Conectar al servidor
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
        printf("Error: No se pudo conectar a %s\n", server);
        exit(1);
    }

    // Enviar archivo
    send_file(sockfd, filename);
    close(sockfd);
    
    printf("Archivo enviado: %s -> %s\n", filename, server);
    log_event(server, filename, "ENVIADO");

    return 0;
}
