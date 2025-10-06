#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>

#define DEFAULT_PORT 49200
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

typedef struct {
    int sockfd;
    char *hostname;
} server_socket_t;

void log_event(const char *server_name, const char *filename, const char *status) {
    time_t now = time(NULL);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    FILE *log_file = fopen("server_log.txt", "a");
    if (log_file) {
        fprintf(log_file, "%s - %s - %s - %s\n", time_str, server_name, filename, status);
        fclose(log_file);
    }
}

void handle_client(int new_socket, const char *hostname) {
    char buffer[BUFFER_SIZE];
    int bytes_received;

    // Crear directorio si no existe
    struct stat st = {0};
    if (stat(hostname, &st) == -1) {
        mkdir(hostname, 0755);
    }

    // Recibir longitud del nombre del archivo
    int filename_len;
    bytes_received = recv(new_socket, &filename_len, sizeof(filename_len), 0);
    if (bytes_received <= 0) {
        close(new_socket);
        return;
    }

    // Recibir nombre del archivo
    char filename[filename_len + 1];
    bytes_received = recv(new_socket, filename, filename_len, 0);
    if (bytes_received <= 0) {
        close(new_socket);
        return;
    }
    filename[filename_len] = '\0';

    // Construir ruta completa
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s", hostname, filename);

    // Recibir archivo
    FILE *file = fopen(filepath, "wb");
    if (!file) {
        log_event(hostname, filename, "ERROR_AL_ABRIR_ARCHIVO");
        close(new_socket);
        return;
    }

    while ((bytes_received = recv(new_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes_received, file);
    }

    fclose(file);
    close(new_socket);
    log_event(hostname, filename, "RECIBIDO_EXITOSAMENTE");
    printf("Archivo %s recibido en %s\n", filename, hostname);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <host1> [host2 ...]\n", argv[0]);
        fprintf(stderr, "Ejemplo Parte A: %s s01 s02\n", argv[0]);
        fprintf(stderr, "Ejemplo Parte B: %s s01 s02 s03 s04\n", argv[0]);
        exit(1);
    }

    server_socket_t *server_sockets = malloc((argc - 1) * sizeof(server_socket_t));
    fd_set readfds;
    int i, max_sd;

    // Crear socket para cada host
    for (i = 1; i < argc; i++) {
        struct hostent *he;
        struct sockaddr_in server_addr;

        if ((he = gethostbyname(argv[i])) == NULL) {
            perror("gethostbyname");
            exit(1);
        }

        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("socket");
            exit(1);
        }

        int opt = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            perror("setsockopt");
            exit(1);
        }

        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(DEFAULT_PORT);
        server_addr.sin_addr = *((struct in_addr *)he->h_addr);

        if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("bind");
            exit(1);
        }

        if (listen(sockfd, MAX_CLIENTS) < 0) {
            perror("listen");
            exit(1);
        }

        server_sockets[i - 1].sockfd = sockfd;
        server_sockets[i - 1].hostname = argv[i];
        printf("Servidor escuchando en %s:%d\n", argv[i], DEFAULT_PORT);
    }

    // Manejar conexiones
    while (1) {
        FD_ZERO(&readfds);
        max_sd = 0;
        
        for (i = 0; i < argc - 1; i++) {
            FD_SET(server_sockets[i].sockfd, &readfds);
            if (server_sockets[i].sockfd > max_sd) {
                max_sd = server_sockets[i].sockfd;
            }
        }

        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("select");
            continue;
        }

        for (i = 0; i < argc - 1; i++) {
            if (FD_ISSET(server_sockets[i].sockfd, &readfds)) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int new_socket = accept(server_sockets[i].sockfd, 
                                      (struct sockaddr *)&client_addr, 
                                      &client_len);
                if (new_socket < 0) {
                    perror("accept");
                    continue;
                }

                printf("Conexión aceptada en %s\n", server_sockets[i].hostname);
                
                if (fork() == 0) {
                    close(server_sockets[i].sockfd);
                    handle_client(new_socket, server_sockets[i].hostname);
                    exit(0);
                }
                close(new_socket);
            }
        }
    }

    free(server_sockets);
    return 0;
}

