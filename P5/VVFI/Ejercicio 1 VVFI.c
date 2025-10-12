// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <pthread.h>

#define BUFFER_SIZE 2048  

// Variables globales
int servidor_ocupado = 0;
int turno_actual = 0;
int total_servidores = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

// Cifrado César
void encryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            text[i] = ((c - 'A' + shift) % 26) + 'A';
        } else if (islower(c)) {
            text[i] = ((c - 'a' + shift) % 26) + 'a';
        }
    }
}

// Estructura para pasar datos al hilo
typedef struct {
    int client_fd;
    int id_servidor;
    int argc;
    char **argv;
} thread_args_t;

// Función que atiende a cada cliente
void* manejar_cliente(void* arg) {
    thread_args_t *data = (thread_args_t*)arg;
    int client_fd = data->client_fd;
    int id_servidor = data->id_servidor;
    int argc = data->argc;
    char **argv = data->argv;

    char buffer[BUFFER_SIZE];
    int bytes_received;

    // Control de turno (Round Robin) 
    pthread_mutex_lock(&mutex);
    while (id_servidor != turno_actual || servidor_ocupado) {
        pthread_cond_wait(&cond, &mutex);
    }

    // Marcar servidor como ocupado
    servidor_ocupado = 1;
    printf("[Servidor %d] Estoy recibiendo archivo... los demás deben esperar.\n", id_servidor);
    pthread_mutex_unlock(&mutex);

    // Recepción del archivo 
    bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        perror("[-] Error al recibir");
        close(client_fd);
        free(data);
        pthread_exit(NULL);
    }
    buffer[bytes_received] = '\0';

    // Formato esperado: "<shift> <texto>"
    int shift;
    char texto[BUFFER_SIZE];
    if (sscanf(buffer, "%d %[^\t\n]", &shift, texto) < 2) {
        fprintf(stderr, "[-] Mensaje recibido con formato incorrecto\n");
        close(client_fd);
        free(data);
        pthread_exit(NULL);
    }

    // Aplicar cifrado
    encryptCaesar(texto, shift);

    // Guardar resultado en cada alias
    for (int i = 2; i < argc - 2; i++) {
        char ruta_archivo[BUFFER_SIZE];
        snprintf(ruta_archivo, sizeof(ruta_archivo), "%s/resultado.txt", argv[i]);
        FILE *fp = fopen(ruta_archivo, "a");
        if (fp) {
            fprintf(fp, "%s\n", texto);
            fclose(fp);
        } else {
            perror("[-] Error al abrir archivo de salida");
        }
    }

    // Responder al cliente
    char respuesta[BUFFER_SIZE];
    snprintf(respuesta, sizeof(respuesta), "Procesado por servidor #%d: %s", id_servidor, texto);
    send(client_fd, respuesta, strlen(respuesta), 0);
    close(client_fd);

    // Liberar servidor y pasar turno 
    pthread_mutex_lock(&mutex);
    servidor_ocupado = 0;
    turno_actual = (turno_actual + 1) % total_servidores;
    printf("[Servidor %d] Archivo recibido completo. Turno pasa al servidor %d.\n",
           id_servidor, turno_actual);
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mutex);

    free(data);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Uso: %s <id_servidor> <alias1> [alias2 ... aliasN] <total_servidores> <puerto>\n", argv[0]);
        return 1;
    }

    // Identificador del servidor y número total de servidores
    int id_servidor = atoi(argv[1]);
    total_servidores = atoi(argv[argc - 2]);
    int puerto_escucha = atoi(argv[argc - 1]);

    // Crear directorios para cada alias
    for (int i = 2; i < argc - 2; i++) {
        if (mkdir(argv[i], 0777) < 0 && errno != EEXIST) {
            perror("[-] Error al crear directorio");
        }
    }

    int server_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // Crear socket TCP
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("[-] Error al crear socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Configuración del servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(puerto_escucha);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] Error en bind");
        close(server_fd);
        exit(1);
    }

    if (listen(server_fd, 10) < 0) {
        perror("[-] Error en listen");
        close(server_fd);
        exit(1);
    }

    printf("[+] Servidor #%d escuchando en puerto %d (aliases:", id_servidor, puerto_escucha);
    for (int i = 2; i < argc - 2; i++) printf(" %s", argv[i]);
    printf(" )\n");

    while (1) {
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            perror("[-] Error en accept");
            continue;
        }

        // Crear un hilo para manejar al cliente
        pthread_t tid;
        thread_args_t *args = malloc(sizeof(thread_args_t));
        args->client_fd = client_fd;
        args->id_servidor = id_servidor;
        args->argc = argc;
        args->argv = argv;
        pthread_create(&tid, NULL, manejar_cliente, args);
        pthread_detach(tid);
    }

    close(server_fd);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);
    return 0;
}
