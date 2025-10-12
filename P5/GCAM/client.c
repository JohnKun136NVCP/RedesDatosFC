#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
#include <arpa/inet.h> 

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
void send_file(int socket, const char *filename, const char *server_name) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Error al abrir archivo");
        log_event(server_name, filename, "ERROR_AL_ABRIR_ARCHIVO");
        exit(1);
    }

    // Extraer solo el nombre base del archivo
    const char *basename = strrchr(filename, '/');
    if (basename) basename++;
    else basename = filename;

    printf("Enviando archivo: %s\n", basename);

    int filename_len = strlen(basename);
    int net_filename_len = htonl(filename_len); 
    
    if (send(socket, &net_filename_len, sizeof(net_filename_len), 0) < 0) {
        perror("Error enviando longitud del nombre");
        fclose(file);
        exit(1);
    }
    
    if (send(socket, basename, filename_len, 0) < 0) {
        perror("Error enviando nombre del archivo");
        fclose(file);
        exit(1);
    }


    // Enviar el contenido del archivo por bloques
    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    size_t total_sent = 0;
    
    // Obtener tamaño del archivo para progreso
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        ssize_t sent = send(socket, buffer, bytes_read, 0);
        if (sent < 0) {
            perror("Error enviando datos del archivo");
            break;
        }
        total_sent += sent;
        
        fflush(stdout);
    }

    fclose(file);
    
    if (total_sent == file_size) {
        printf("\nArchivo enviado completamente: %s -> %s \n", 
               basename, server_name);
        log_event(server_name, filename, "ENVIADO_EXITOSAMENTE");
    } else {
        printf("\nArchivo enviado parcialmente: %s -> %s (%zu/%ld bytes)\n", 
               basename, server_name, total_sent, file_size);
        log_event(server_name, filename, "ENVIADO_PARCIALMENTE");
    }
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
            printf("[%s] Inactivo (no se pudo resolver)\n", server);
            log_event(server, "status", "INACTIVO_NO_RESUELTO");
            exit(1);
        }

        // Crear socket
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            printf("[%s] Inactivo (error creando socket)\n", server);
            log_event(server, "status", "INACTIVO_ERROR_SOCKET");
            exit(1);
        }

        // Configurar dirección del servidor
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        server_addr.sin_addr = *((struct in_addr *)he->h_addr);
        memset(&(server_addr.sin_zero), '\0', 8);

        // Definir tiempo de espera
        struct timeval timeout = {2, 0};
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        // Intentar conexión
        if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
            printf("[%s] Inactivo (no se pudo conectar)\n", server);
            log_event(server, "status", "INACTIVO_SIN_CONEXION");
            close(sockfd);
            exit(1);
        }

        printf("[%s] Activo\n", server);
        log_event(server, "status", "ACTIVO");
        close(sockfd);
        return 0;
    }

    // Envio de archivo
    struct hostent *he;
    struct sockaddr_in server_addr;
    int sockfd;

    printf("Conectando a %s:%d...\n", server, port);

    // Resolver host
    if ((he = gethostbyname(server)) == NULL) {
        printf("Error: No se pudo resolver %s\n", server);
        log_event(server, filename, "ERROR_RESOLUCION_HOST");
        exit(1);
    }

    // Crear socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("Error: No se pudo crear socket\n");
        log_event(server, filename, "ERROR_CREACION_SOCKET");
        exit(1);
    }

    // Configurar dirección del servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr = *((struct in_addr *)he->h_addr);
    memset(&(server_addr.sin_zero), '\0', 8);

    // Conectar al servidor
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
        printf("Error: No se pudo conectar a %s:%d\n", server, port);
        log_event(server, filename, "ERROR_CONEXION");
        close(sockfd);
        exit(1);
    }

    // Enviar archivo
    send_file(sockfd, filename, server);
    close(sockfd);

    return 0;
}