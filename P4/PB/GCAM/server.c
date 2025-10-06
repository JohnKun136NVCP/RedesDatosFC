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
#include <errno.h>

#define DEFAULT_PORT 49200   // Puerto por defecto
#define MAX_CLIENTS 10       // Número máximo de clientes
#define BUFFER_SIZE 1024    

// Estructura para almacenar información de cada servidor
typedef struct {
    int sockfd;
    char *hostname;
    char ip[16];
} server_socket_t;

// Registrar eventos en un archivo de log
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

// Crea un directorio si no existe 
void create_directory(const char *dirname) {
    struct stat st = {0};
    if (stat(dirname, &st) == -1) {
        mkdir(dirname, 0755);
    }
}

// Manejar la conexión con un cliente
void handle_client(int new_socket, const char *hostname) {
    char buffer[BUFFER_SIZE];
    int bytes_received;

    // Crear carpeta con nombre del host
    create_directory(hostname);

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

    // Ruta completa para guardar el archivo
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s", hostname, filename);

    // Abrir archivo destino
    FILE *file = fopen(filepath, "wb");
    if (!file) {
        log_event(hostname, filename, "ERROR_AL_ABRIR_ARCHIVO");
        close(new_socket);
        return;
    }

    // Recibir datos y escribir en el archivo
    size_t total_bytes = 0;
    while ((bytes_received = recv(new_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes_received, file);
        total_bytes += bytes_received;
    }

    fclose(file);
    close(new_socket);
    
    printf("[%s] Archivo recibido: %s (%zu bytes)\n", hostname, filename, total_bytes);
    log_event(hostname, filename, "RECIBIDO_EXITOSAMENTE");
}

// Crear un socket de servidor para un host y su IP
int create_server_socket(const char *hostname, const char *ip) {
    struct sockaddr_in server_addr;
    
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return -1;
    }

    // Permitir reutilización del puerto
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(sockfd);
        return -1;
    }

    // Configurar dirección del servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DEFAULT_PORT);
    
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        close(sockfd);
        return -1;
    }

    // Asociar socket a dirección e iniciar escucha
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(sockfd);
        return -1;
    }

    if (listen(sockfd, MAX_CLIENTS) < 0) {
        close(sockfd);
        return -1;
    }

    return sockfd;
}

// Resolver un hostname en dirección IP
int resolve_hostname(const char *hostname, char *ip_buffer) {
    struct hostent *he;
    
    if ((he = gethostbyname(hostname)) == NULL) {
        return -1;
    }
    
    struct in_addr **addr_list = (struct in_addr **)he->h_addr_list;
    if (addr_list[0] != NULL) {
        strcpy(ip_buffer, inet_ntoa(*addr_list[0]));
        return 0;
    }
    
    return -1;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <host1> [host2 ...]\n", argv[0]);
        exit(1);
    }

    // Reservar memoria para los servidores
    server_socket_t *server_sockets = malloc((argc - 1) * sizeof(server_socket_t));
    if (!server_sockets) {
        exit(1);
    }

    int valid_servers = 0;

    // Crear socket para cada host recibido por parámetro
    for (int i = 1; i < argc; i++) {
        char ip[16];
        
        if (resolve_hostname(argv[i], ip) == 0) {
            int sockfd = create_server_socket(argv[i], ip);
            if (sockfd >= 0) {
                server_sockets[valid_servers].sockfd = sockfd;
                server_sockets[valid_servers].hostname = argv[i];
                strcpy(server_sockets[valid_servers].ip, ip);
                
                printf("Servidor listo: %s (%s:%d)\n", argv[i], ip, DEFAULT_PORT);
                valid_servers++;
            }
        }
    }

    if (valid_servers == 0) {
        fprintf(stderr, "Error: No se pudo iniciar ningún servidor\n");
        free(server_sockets);
        exit(1);
    }

    printf("\nServidores activos: %d\n", valid_servers);
    signal(SIGCHLD, SIG_IGN); // Ignorar procesos hijos terminados

    // Aceptar conexiones en múltiples servidores
    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        int max_sd = 0;
        
        // Agregar todos los sockets al conjunto de lectura
        for (int i = 0; i < valid_servers; i++) {
            FD_SET(server_sockets[i].sockfd, &readfds);
            if (server_sockets[i].sockfd > max_sd) {
                max_sd = server_sockets[i].sockfd;
            }
        }

        // Esperar actividad en algún socket
        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) {
            continue;
        }

        // Revisar en qué servidor llegó la conexión
        for (int i = 0; i < valid_servers; i++) {
            if (FD_ISSET(server_sockets[i].sockfd, &readfds)) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                
                int new_socket = accept(server_sockets[i].sockfd, 
                                      (struct sockaddr *)&client_addr, 
                                      &client_len);
                if (new_socket < 0) {
                    continue;
                }

                // Obtener IP del cliente
                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
                
                // Crear proceso hijo para atender al cliente
                pid_t pid = fork();
                if (pid == 0) {
                    // Hijo: cerrar sockets del padre
                    for (int j = 0; j < valid_servers; j++) {
                        close(server_sockets[j].sockfd);
                    }
                    handle_client(new_socket, server_sockets[i].hostname);
                    exit(0);
                } else if (pid > 0) {
                    // Padre: cerrar socket del cliente
                    close(new_socket);
                } else {
                    close(new_socket);
                }
            }
        }
    }

    free(server_sockets);
    return 0;
}
