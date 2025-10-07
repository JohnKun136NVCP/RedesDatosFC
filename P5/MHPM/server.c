#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>

#define BUFFER_SIZE 2048
#define PORT 49200
#define TURN_TIME 2 // Tiempo de cada turno

// Variables compartidas para el control de turnos
typedef struct {
    int current_turn;
    int num_servers;
    int receiving; // Indica si algún servidor está recibiendo
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} TurnControl;

TurnControl *turn_control;

// Función para obtener la fecha y hora 
void get_timestamp(char *timestamp) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(timestamp, 20, "%Y-%m-%d %H:%M:%S", t);
}

// Función para verificar si hay conexiones pendientes
int check_pending_connections(int server_sock) {
    fd_set readfds;
    struct timeval tv;
    
    FD_ZERO(&readfds);
    FD_SET(server_sock, &readfds);
    
    tv.tv_sec = 0;
    tv.tv_usec = 100000;  // 100ms timeout
    
    int result = select(server_sock + 1, &readfds, NULL, NULL, &tv);
    return (result > 0);
}

// Hilo para gestionar el sistema Round Robin
void* round_robin_controller(void* arg) {
    int num_servers = *((int*)arg);
    
    while (1) {
        sleep(TURN_TIME); // Esperar el tiempo del turno
        
        pthread_mutex_lock(&turn_control->mutex);
        
        // Si nadie está recibiendo, avanzar al siguiente turno
        if (!turn_control->receiving) {
            turn_control->current_turn = (turn_control->current_turn + 1) % num_servers;
            printf("[Control] Turno asignado al servidor %d\n", turn_control->current_turn);
            pthread_cond_broadcast(&turn_control->cond);
        } else {
            printf("[Control] Servidor %d está recibiendo, manteniendo turno\n", 
                   turn_control->current_turn);
        }
        
        pthread_mutex_unlock(&turn_control->mutex);
    }
    
    return NULL;
}

// Función que maneja cada servidor en un alias específico
void run_server(char *alias, int server_id, int num_servers) {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    char buffer[BUFFER_SIZE];
    char timestamp[20];

    // Crear socket TCP
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("[-] socket");
        exit(1);
    }

    // Configurar socket como no bloqueante
    int flags = fcntl(server_sock, F_GETFL, 0);
    fcntl(server_sock, F_SETFL, flags | O_NONBLOCK);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Permitir reuso de dirección
    int opt = 1;
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("[-] setsockopt");
        close(server_sock);
        exit(1);
    }

    // Asociar el socket con la dirección y puerto
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] bind");
        close(server_sock);
        exit(1);
    }

    // Escuchar conexiones (hasta 10 en cola)
    if (listen(server_sock, 10) < 0) {
        perror("[-] listen");
        close(server_sock);
        exit(1);
    }
    
    printf("[Servidor %s ID:%d] Escuchando en puerto %d (PID: %d)\n", 
           alias, server_id, PORT, getpid());

    // Aceptar conexiones entrantes
    while (1) {
        pthread_mutex_lock(&turn_control->mutex);

        // Esperar hasta que sea el turno de este servidor
        while (turn_control->current_turn != server_id || turn_control->receiving) {
            pthread_cond_wait(&turn_control->cond, &turn_control->mutex);
        }
        
        printf("[Servidor %s ID:%d] Es mi turno\n", alias, server_id);

        // Verificar si hay conexiones pendientes
        if (!check_pending_connections(server_sock)) {
            printf("[Servidor %s ID:%d] No hay conexiones pendientes, cedo mi turno\n", 
                   alias, server_id);
            pthread_mutex_unlock(&turn_control->mutex);
            sleep(1);
            continue;
        }

        // Marcar que estamos recibiendo
        turn_control->receiving = 1;
        printf("[Servidor %s ID:%d] Iniciando recepción, bloqueando otros servidores\n", 
               alias, server_id);
        pthread_mutex_unlock(&turn_control->mutex);
        
        // Aceptar conexión
        addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
        if (client_sock < 0) {
            pthread_mutex_lock(&turn_control->mutex);
            turn_control->receiving = 0;
            pthread_cond_broadcast(&turn_control->cond);
            pthread_mutex_unlock(&turn_control->mutex);
            continue;
        }

        // Recibir datos del cliente
        int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            get_timestamp(timestamp);

            printf("[Servidor %s ID:%d] %s - Archivo recibido (%d bytes) de: %s\n", 
                   alias, server_id, timestamp, bytes, inet_ntoa(client_addr.sin_addr));

            // Enviar confirmación
            char response[BUFFER_SIZE];
            snprintf(response, sizeof(response), "OK:%s:%s:%d", alias, timestamp, bytes);
            send(client_sock, response, strlen(response), 0);
        }
       
        close(client_sock);

        // Liberar el bloqueo de recepción
        pthread_mutex_lock(&turn_control->mutex);
        turn_control->receiving = 0;
        printf("[Servidor %s ID:%d] Recepción completada, liberando bloqueo\n", 
               alias, server_id);
        pthread_cond_broadcast(&turn_control->cond);
        pthread_mutex_unlock(&turn_control->mutex);
        
        usleep(500000);  // Pausa de 0.5s
    }

    close(server_sock);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Uso: %s <alias1> <alias2> ...\n", argv[0]);
        return 1;
    }

    int num_servers = argc - 1;

    // Inicializar control de turnos en memoria compartida
    turn_control = mmap(NULL, sizeof(TurnControl), 
                       PROT_READ | PROT_WRITE, 
                       MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    
    turn_control->current_turn = 0;
    turn_control->num_servers = num_servers;
    turn_control->receiving = 0;

    // Configurar mutex para procesos compartidos
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&turn_control->mutex, &mutex_attr);
    
    // Configurar condition variable para procesos compartidos
    pthread_condattr_t cond_attr;
    pthread_condattr_init(&cond_attr);
    pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&turn_control->cond, &cond_attr);

    printf("[Servidor Principal] Iniciando %d servidores con Round Robin...\n", num_servers);
    printf("[Servidor Principal] Tiempo por turno: %d segundos\n", TURN_TIME);
    
    // Crear proceso para el controlador Round Robin
    pid_t controller_pid = fork();
    if (controller_pid == 0) {
        // Proceso hijo - controlador de turnos
        round_robin_controller(&num_servers);
        exit(0);
    }

    // Crear procesos hijos para cada servidor
    for (int i = 0; i < num_servers; i++) {
        pid_t pid = fork();
        
        if (pid == 0) { 
            // Proceso hijo - servidor
            run_server(argv[i + 1], i, num_servers);
            exit(0);
        } else if (pid < 0) {
            perror("[-] fork");
            exit(1);
        } else {
            printf("[Servidor Principal] Servidor %s iniciado con PID: %d\n", argv[i + 1], pid);
        }
    }

    // Esperar a que todos los procesos hijos terminen
    while (wait(NULL) > 0);

    // Limpiar recursos
    pthread_mutex_destroy(&turn_control->mutex);
    pthread_cond_destroy(&turn_control->cond);
    munmap(turn_control, sizeof(TurnControl));

    return 0;
}