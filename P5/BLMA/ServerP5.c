#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>

#define PORT 49200
#define BUFFER_SIZE 1024
#define NUM_SERVERS 4 
#define RR_TIMEOUT_SECONDS 5 // tiempo que dura el turno
#define LOG_FILE "server_logs.txt"

// variables globales de sincronización
pthread_mutex_t recv_mutex = PTHREAD_MUTEX_INITIALIZER; // mutex: protege la recepción (exclusión mutua)
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;  // mutex: protege el acceso concurrente al log
int current_turn_index = 0; // índice que indica qué alias tiene el turno
char *aliases[NUM_SERVERS] = {"s01", "s02", "s03", "s04"}; // servidores que participan en el round robin

// estructura para pasar datos al hilo del cliente
typedef struct {
    int socket;
} client_data_t;


// función para registrar logs
void registrar_log(const char *cliente, const char *operacion, const char *archivo,
                   long bytes, double duracion, const char *estado) {
    
    pthread_mutex_lock(&log_mutex); // bloquea el log
    
    FILE *log_file = fopen(LOG_FILE, "a");
    if (!log_file) {
        perror("Error al abrir log");
        pthread_mutex_unlock(&log_mutex); 
        return;
    }

    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    fprintf(log_file, "%04d-%02d-%02d, %02d:%02d:%02d, %s, %s, %s, %ld, %.2fs, %s\n",
            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
            t->tm_hour, t->tm_min, t->tm_sec,
            cliente, operacion, archivo, bytes, duracion, estado);

    fclose(log_file);
    pthread_mutex_unlock(&log_mutex); // libera el log
}

// hilo de coordinación round robin
void *round_robin_scheduler(void *arg) {
    printf("[coordinador] hilo round robin iniciado. turno actual: %s\n", aliases[current_turn_index]);
    while (1) {
        sleep(RR_TIMEOUT_SECONDS); // espera el tiempo de turno

        pthread_mutex_lock(&recv_mutex); // bloquea momentáneamente el mutex para actualizar el turno
        current_turn_index = (current_turn_index + 1) % NUM_SERVERS; // rota al siguiente alias
        printf("\n[coordinador] nuevo turno asignado a: %s\n", aliases[current_turn_index]);
        pthread_mutex_unlock(&recv_mutex); // libera el mutex
    }
    return NULL;
}

// hilo de manejo de cliente
void *handle_client(void *arg) {
    client_data_t *data = (client_data_t*)arg;
    int new_socket = data->socket;
    
    char alias_cliente[100];
    char nombre_archivo[100];
    
    memset(alias_cliente, 0, sizeof(alias_cliente)); // inicializa buffer a cero
    memset(nombre_archivo, 0, sizeof(nombre_archivo)); // inicializa buffer a cero

    read(new_socket, alias_cliente, 99); // lee alias del cliente
    read(new_socket, nombre_archivo, 99); // lee nombre del archivo

    alias_cliente[strcspn(alias_cliente, "\0")] = 0; // limpia la cadena
    nombre_archivo[strcspn(nombre_archivo, "\0")] = 0;

    int client_turn = -1;
    for (int i = 0; i < NUM_SERVERS; i++) {
        if (strcmp(alias_cliente, aliases[i]) == 0) {
            client_turn = i;
            break;
        }
    }

    // lógica de restricción: evalúa el turno (round robin) y la disponibilidad (mutex)
    if (client_turn != current_turn_index || pthread_mutex_trylock(&recv_mutex) != 0) {
        
        // rechazo: no tiene el turno o el mutex está ocupado
        char full_msg[200];
        if (client_turn != current_turn_index) {
            snprintf(full_msg, 200, "rechazado: no es tu turno. espera por el turno de %s.", aliases[current_turn_index]);
        } else {
            snprintf(full_msg, 200, "rechazado: servidor ocupado. otro cliente esta recibiendo.");
        }
        
        send(new_socket, full_msg, strlen(full_msg), 0);
        registrar_log(alias_cliente, "recepción", nombre_archivo, 0, 0.0, "en espera (rechazo)");
        printf("[rechazo] cliente %s rechazado. turno actual: %s.\n", alias_cliente, aliases[current_turn_index]);

    } else {
        // aceptación: tiene el turno y adquiere el mutex
        printf("[recepción] cliente %s aceptado. iniciando transferencia...\n", alias_cliente);
        registrar_log(alias_cliente, "recepción", nombre_archivo, 0, 0.0, "recibiendo archivo");
        
        // simulación de recepción (aquí iría el bucle real de read)
        usleep(1000000); 
        
        send(new_socket, "archivo recibido con éxito y turno completado.", strlen("archivo recibido con éxito y turno completado."), 0);
        registrar_log(alias_cliente, "recepción", nombre_archivo, 2048, 1.0, "completado");
        
        pthread_mutex_unlock(&recv_mutex); // libera el mutex
    }

    close(new_socket);
    free(arg); 
    return NULL;
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int opt = 1;

    // inicialización de socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Error al crear socket");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Error en bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0) {
        perror("Error en listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // iniciar hilo rr
    pthread_t rr_thread;
    if (pthread_create(&rr_thread, NULL, round_robin_scheduler, NULL) < 0) {
        perror("Error al crear hilo rr");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    pthread_detach(rr_thread); 

    printf("servidor v5 en espera en puerto %d...\n", PORT);
    registrar_log("-", "servidor", "inicio", 0, 0.0, "en espera");

    while (1) {
        // bucle principal: acepta cualquier conexión entrante
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("Error en accept");
            continue;
        }

        // crea estructura de datos y lanza un hilo por cada cliente
        client_data_t *data = (client_data_t*)malloc(sizeof(client_data_t));
        if (data == NULL) {
            perror("Error al asignar memoria para thread data");
            close(new_socket);
            continue;
        }
        data->socket = new_socket;
        
        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, handle_client, (void*)data) < 0) {
            perror("Error al crear hilo cliente");
            close(new_socket);
            free(data);
        }
        pthread_detach(client_thread); 
    }
    
    close(server_fd);
    return 0;
}