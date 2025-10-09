/* Ejercicio 1, Práctica 5: server.c
 * Elaborado por Alejandro Axel Rodríguez Sánchez
 * ahexo@ciencias.unam.mx
 *
 * Facultad de Ciencias UNAM
 * Redes de computadoras
 * Grupo 7006
 * Semestre 2026-1
 * 2025-10-06
 *
 * Profesor:
 * Luis Enrique Serrano Gutiérrez
 * Ayudante de laboratorio:
 * Juan Angeles Hernández
 */

// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <libgen.h>

// Para la práctica 5
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>       /* Para semáforos POSIX */
#include <sys/select.h>      /* Para select() */

#define PORT "49200"
#define BACKLOG 10
#define BUFFER_SIZE 4096
#define SEM_PREFIX "/turn_sem_server_" // Prefijo para los nombres de los semáforos
#define QUANTUM_SECONDS 5 // En segundos

// --- Variables globales para limpieza ---
// Necesitamos los nombres para poder borrarlos al final
char** g_sem_names = NULL;
int g_num_servers = 0;

// Función para gestionar una conexión
void handle_connection(int client_fd, const char *hostname) {
    char buffer[BUFFER_SIZE];
    time_t now;
    // Enviamos una confirmación de conexión al cliente
    time(&now);
    // Quitamos el newline de ctime
    char* time_str = ctime(&now);
    time_str[strlen(time_str) - 1] = '\0';

    int len = snprintf(buffer, BUFFER_SIZE, "[%s] Bienvenido a %s, la conexión se ha establecido exitosamente.", time_str, hostname);
    if (send(client_fd, buffer, len, 0) == -1) {
        perror("send welcome");
        close(client_fd);
        exit(EXIT_FAILURE);
    }
    printf("[%s] Se mandó un mensaje de confirmación al cliente.\n", hostname);

    // Creamos el directorio del host si no existe
    char dir_path[512];
    const char *homedir = getenv("HOME");
    snprintf(dir_path, sizeof(dir_path), "%s/%s", homedir, hostname);
    mkdir(dir_path, 0755); // Crea el directorio, no falla si ya existe

    // Recibimos un archivo del cliente
    memset(buffer, 0, BUFFER_SIZE);
    ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
    if (bytes_read <= 0) {
        fprintf(stderr, "[%s] Error: No se ha mandado ningún archivo\n", hostname);
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    // Parseamos el nombre del archivo
    char filename[256];
    strncpy(filename, buffer, sizeof(filename) - 1);
    filename[sizeof(filename) - 1] = '\0';
    printf("[%s] Recibiendo archivo: %s\n", hostname, filename);

    // Guardamos espacio para el archivo
    char fpath[1024];
    snprintf(fpath, sizeof(fpath), "%s/%s", dir_path, basename(filename));

    FILE *fp = fopen(fpath, "wb");
    if (fp == NULL) {
        perror("fopen");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    // Recibimos el archivo
    printf("[%s] Guardando archivo en %s\n", hostname, fpath);
    char *file_content_start = buffer + strlen(filename) + 1;
    size_t initial_data_len = bytes_read - (file_content_start - buffer);
    if (initial_data_len > 0) {
        fwrite(file_content_start, 1, initial_data_len, fp);
    }
    while ((bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes_read, fp);
    }
    fclose(fp);
    printf("[%s] Archivo guardado: %s\n", hostname, filename);

    // Le mandamos un mensaje de confirmación al cliente
    const char *success_msg = "El archivo se ha guardado exitosamente en el servidor.\n";
    if (send(client_fd, success_msg, strlen(success_msg), 0) == -1) {
        perror("send success");
    }

    close(client_fd);
    exit(EXIT_SUCCESS);
}

// --- FUNCIÓN MODIFICADA PARA LA LÓGICA DE TURNOS ---
void run_server(const char *hostname, int server_index, int total_servers) {
    struct addrinfo hints, *res;
    int sockfd, client_fd;
    socklen_t sin_size;
    struct sockaddr_storage their_addr;
    int yes = 1;

    // Abrimos los semáforos necesarios
    sem_t *my_turn_sem, *next_turn_sem;
    char my_sem_name[256], next_sem_name[256];

    snprintf(my_sem_name, sizeof(my_sem_name), "%s%d", SEM_PREFIX, server_index);
    int next_server_index = (server_index + 1) % total_servers;
    snprintf(next_sem_name, sizeof(next_sem_name), "%s%d", SEM_PREFIX, next_server_index);

    // Abrimos el semáforo que nos da el turno
    if ((my_turn_sem = sem_open(my_sem_name, 0)) == SEM_FAILED) {
        perror("sem_open (my_turn)");
        exit(EXIT_FAILURE);
    }
    // Abrimos el semáforo del siguiente servidor para cederle el turno
    if ((next_turn_sem = sem_open(next_sem_name, 0)) == SEM_FAILED) {
        perror("sem_open (next_turn)");
        exit(EXIT_FAILURE);
    }

    // Configuración del socket
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(hostname, PORT, &hints, &res) != 0) {
        fprintf(stderr, "Error resolviendo el host: %s\n", hostname);
        exit(1);
    }

    if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
        perror("socket");
        exit(1);
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("setsockopt");
        exit(1);
    }

    if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
        close(sockfd);
        perror("bind");
        exit(1);
    }

    freeaddrinfo(res);

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    signal(SIGCHLD, SIG_IGN);
    printf("Servidor en '%s' (índice %d) listo y esperando su turno...\n", hostname, server_index);

    // Bucle de ejecución
    while (1) {
        printf("[%s] Esperando turno...\n", hostname);

        // 1. Esperar a que sea nuestro turno (sem_wait se bloquea hasta que el semáforo es > 0)
        sem_wait(my_turn_sem);

        printf("[%s] Turno iniciado. Escuchando conexiones por %d segundos.\n", hostname, QUANTUM_SECONDS);

        // 2. Usar select() para esperar una conexión durante el quantum
        fd_set read_fds;
        struct timeval tv;
        int retval;

        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);

        tv.tv_sec = QUANTUM_SECONDS;
        tv.tv_usec = 0;

        retval = select(sockfd + 1, &read_fds, NULL, NULL, &tv);

        if (retval == -1) {
            perror("select()");
        } else if (retval > 0) {
            // Hay una conexión entrante
            printf("[%s] Conexión detectada. Aceptando...\n", hostname);
            sin_size = sizeof their_addr;
            client_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
            if (client_fd == -1) {
                perror("accept");
            } else {
                if (!fork()) { // Fork para manejar la conexión
                    close(sockfd);
                    // Una vez aceptada, se gestiona hasta el final sin importar el tiempo
                    handle_connection(client_fd, hostname);
                }
                close(client_fd);
            }
        } else {
            // select() devolvió 0, significa que se agotó el tiempo.
            printf("[%s] No hubo conexiones en este turno. Cediendo el turno.\n", hostname);
        }

        // 3. Ceder el turno al siguiente servidor
        printf("[%s] Turno finalizado. Avisando al siguiente (%d).\n\n", hostname, next_server_index);
        sem_post(next_turn_sem);
    }

    // Limpiamos al final de cada ejecución del while
    sem_close(my_turn_sem);
    sem_close(next_turn_sem);
    close(sockfd);
}

// Para cerrar conexiones
void cleanup(int sig) {
    printf("\nCerrando servidores y limpiando semáforos...\n");
    if (g_sem_names) {
        for (int i = 0; i < g_num_servers; i++) {
            sem_unlink(g_sem_names[i]);
            free(g_sem_names[i]);
        }
        free(g_sem_names);
    }
    // Matar a todos los procesos hijos
    kill(0, SIGKILL);
    exit(0);
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <hostname1> [hostname2] ...\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s s01 s02 s03 s04\n", argv[0]);
        exit(1);
    }

    g_num_servers = argc - 1;
    g_sem_names = malloc(g_num_servers * sizeof(char*));

    // Iniciamos los semáforos
    for (int i = 0; i < g_num_servers; i++) {
        char sem_name[256];
        snprintf(sem_name, sizeof(sem_name), "%s%d", SEM_PREFIX, i);
        g_sem_names[i] = strdup(sem_name);

        // Borramos semáforos previos por si el programa no cerró bien antes
        sem_unlink(sem_name);

        // El primer semáforo se inicializa en 1 (para que empiece el primer servidor)
        // Los demás se inicializan en 0 (para que esperen)
        int initial_value = (i == 0) ? 1 : 0;
        if (sem_open(sem_name, O_CREAT, 0644, initial_value) == SEM_FAILED) {
            perror("sem_open (main)");
            exit(EXIT_FAILURE);
        }
    }
    printf("Semáforos creados. Iniciando el ciclo de turnos...\n");

    // Manejador de señal para limpieza
    signal(SIGINT, cleanup);

    // Creación de los servidores
    for (int i = 1; i < argc; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) { // Proceso hijo
            // Cada hijo ejecuta su propia instancia de servidor
            // Le pasamos su índice (i-1) y el total de servidores
            run_server(argv[i], i - 1, g_num_servers);
            exit(EXIT_SUCCESS); // No debería llegar aquí
        }
    }

    // El padre espera a todos los hijos (no debería terminar a menos que haya un error)
    for (int i = 1; i < argc; i++) {
        wait(NULL);
    }

    // Limpieza final si todos los hijos terminaran
    cleanup(0);

    return 0;
}
