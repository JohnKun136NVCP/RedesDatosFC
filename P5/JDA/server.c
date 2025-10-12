#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h> // Para hilos
#include <fcntl.h>   // Para hacer el socket no bloqueante
#include <sys/select.h>

#define PORT 49200
#define BUFFER_SIZE 8192

// Estructura para pasar argumentos a cada hilo de servidor
typedef struct {
    char *alias;
    int thread_id;
} server_args_t;

// Estructura de estado compartido entre los hilos
typedef struct {
    pthread_mutex_t lock;       // Mutex para proteger el acceso al estado compartido
    pthread_cond_t turn_cond;   // Variable de condición para manejar los turnos
    int current_turn;           // ID del hilo/servidor que tiene el turno
    int total_servers;          // Número total de servidores
} shared_state_t;

// Variable global para el estado compartido
shared_state_t shared_state;

// Función que ejecutará cada hilo de servidor
void *server_thread_function(void *args) {
    server_args_t *server_info = (server_args_t *)args;
    char *server_alias = server_info->alias;
    int thread_id = server_info->thread_id;

    int servidor_fd, cliente_sock;
    struct sockaddr_in server_addr;
    socklen_t addr_size;

    // 1. Crear el socket del servidor
    servidor_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (servidor_fd == -1) {
        perror("Error al crear el socket");
        pthread_exit(NULL);
    }

    // Configurar la dirección del servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    struct hostent *host_info = gethostbyname(server_alias);
    if (host_info == NULL) {
        fprintf(stderr, "Error: No se pudo resolver el host '%s'. Revisa tu /etc/hosts.\n", server_alias);
        close(servidor_fd);
        pthread_exit(NULL);
    }
    server_addr.sin_addr = *((struct in_addr *)host_info->h_addr_list[0]);
    
    // 2. Enlazar (bind) el socket
    if (bind(servidor_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error en bind. Asegúrate de que el alias y la IP son correctos y no están en uso");
        close(servidor_fd);
        pthread_exit(NULL);
    }

    // 3. Poner el servidor en modo de escucha
    if (listen(servidor_fd, 5) < 0) {
        perror("Error en listen");
        close(servidor_fd);
        pthread_exit(NULL);
    }
    
    // Hacer el socket no bloqueante para poder verificar si hay conexiones sin detenernos
    fcntl(servidor_fd, F_SETFL, O_NONBLOCK);

    printf("[Servidor '%s'] listo y esperando su turno.\n", server_alias);

    // Bucle principal del servidor
    while (1) {
        // SECCION CRITICA DE COORDINACION
        pthread_mutex_lock(&shared_state.lock);

        // Esperar a que sea nuestro turno
        while (shared_state.current_turn != thread_id) {
            pthread_cond_wait(&shared_state.turn_cond, &shared_state.lock);
        }

        printf("\n[Turno para Servidor '%s']\n", server_alias);

        // LOGICA DE RECEPCION (DENTRO DEL TURNO)
        fd_set readfds;
        struct timeval tv;
        int retval;

        FD_ZERO(&readfds);
        FD_SET(servidor_fd, &readfds);

        // Esperar un corto tiempo para ver si hay una conexion
        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        retval = select(servidor_fd + 1, &readfds, NULL, NULL, &tv);

        if (retval == -1) {
            perror("select()");
        } else if (retval > 0) {
            printf("[Servidor '%s'] tiene una conexión en espera. Utilizando su turno.\n", server_alias);
            
            cliente_sock = accept(servidor_fd, NULL, NULL);
            if (cliente_sock < 0) {
                perror("Error en accept");
            } else {
                printf("[Servidor '%s'] cliente conectado.\n", server_alias);
                
                // Logica para recibir el archivo
                char filename[512] = {0};
                char file_content[BUFFER_SIZE] = {0};

                int bytes_recibidos = recv(cliente_sock, filename, sizeof(filename) - 1, 0);
                if(bytes_recibidos > 0) {
                    filename[bytes_recibidos] = '\0';
                    
		    send(cliente_sock, "OK_NAME", strlen("OK_NAME"), 0);
                    
		    bytes_recibidos = recv(cliente_sock, file_content, sizeof(file_content) - 1, 0);
                    if (bytes_recibidos > 0) {
                        file_content[bytes_recibidos] = '\0';

                        char *home_dir = getenv("HOME");
                        char ruta_completa[1024];
                        snprintf(ruta_completa, sizeof(ruta_completa), "%s/%s/%s", home_dir, server_alias, filename);

                        FILE *fp = fopen(ruta_completa, "w");
                        if (fp != NULL) {
                            fputs(file_content, fp);
                            fclose(fp);
                            printf("¡Archivo guardado exitosamente por '%s'!\n", server_alias);
                            send(cliente_sock, "OK: Archivo guardado", strlen("OK: Archivo guardado"), 0);
                        } else {
                            perror("Error al abrir el archivo para escritura");
                        }
                    }
                }
                close(cliente_sock);
            }
        } else {
            // No hay conexiones en espera, cedemos el turno.
            printf("[Servidor '%s'] no tiene recepciones en espera. Cediendo turno.\n", server_alias);
        }

        // PASAR EL TURNO AL SIGUIENTE SERVIDOR
        shared_state.current_turn = (shared_state.current_turn + 1) % shared_state.total_servers;
        
        // Notificar a todos los hilos que el turno ha cambiado
        pthread_cond_broadcast(&shared_state.turn_cond);
        
        // Liberar el mutex
        pthread_mutex_unlock(&shared_state.lock);

        // Pequeña pausa para no saturar la CPU
        usleep(500000); // 0.5 segundos
    }

    close(servidor_fd);
    free(server_info);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <alias_servidor1> <alias_servidor2> ...\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int num_servers = argc - 1;
    pthread_t threads[num_servers];

    // Inicializar el estado compartido
    pthread_mutex_init(&shared_state.lock, NULL);
    pthread_cond_init(&shared_state.turn_cond, NULL);
    shared_state.current_turn = 0; // El primer servidor empieza
    shared_state.total_servers = num_servers;

    // Crear un hilo por cada alias de servidor
    for (int i = 0; i < num_servers; i++) {
        server_args_t *args = malloc(sizeof(server_args_t));
        args->alias = argv[i + 1];
        args->thread_id = i;
        
        if (pthread_create(&threads[i], NULL, server_thread_function, (void *)args) != 0) {
            perror("Error al crear el hilo");
            exit(EXIT_FAILURE);
        }
    }

    // Esperar a que todos los hilos terminen (en este caso, nunca, ya que es un bucle infinito)
    for (int i = 0; i < num_servers; i++) {
        pthread_join(threads[i], NULL);
    }

    // Destruir el mutex y la variable de condicion
    pthread_mutex_destroy(&shared_state.lock);
    pthread_cond_destroy(&shared_state.turn_cond);

    return 0;
}
