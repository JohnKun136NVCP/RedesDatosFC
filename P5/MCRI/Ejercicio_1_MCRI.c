#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <netdb.h>
#include <time.h>
#include <signal.h>

#define MAIN_PORT 49200
#define BUFFER_SIZE 1024
#define QUANTUM 10

char **alias_list;
int num_alias;
int current_turn = 0;

pthread_mutex_t turn_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t turn_cond = PTHREAD_COND_INITIALIZER;

volatile sig_atomic_t stop = 0;

void get_datetime(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", t);
}

void *handle_client(void *arg) {
    int client_sock = *(int *)arg;
    free(arg);
    char buffer[BUFFER_SIZE];
    int bytes;

    // Recibir mensajes
    while ((bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes] = '\0';
        char datetime[64];
        get_datetime(datetime, sizeof(datetime));
        printf("[%s] Mensaje recibido: %s\n", datetime, buffer);
        fflush(stdout);
    }

    close(client_sock);
    pthread_exit(NULL);
}

void *server_thread(void *arg) {
    int my_id = *(int *)arg;
    char *alias = alias_list[my_id];
    struct addrinfo hints, *res;
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", MAIN_PORT);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // Obtener dirección IP del alias
    if (getaddrinfo(alias, port_str, &hints, &res) != 0) {
        perror("[-] Error al resolver alias");
        pthread_exit(NULL);
    }

    // Crear socket
    int server_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (server_sock < 0) {
        perror("[-] Error al crear socket servidor");
        freeaddrinfo(res);
        pthread_exit(NULL);
    }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Enlazar socket
    if (bind(server_sock, res->ai_addr, res->ai_addrlen) < 0) {
        perror("[-] Error en bind");
        close(server_sock);
        freeaddrinfo(res);
        pthread_exit(NULL);
    }

    freeaddrinfo(res);

    // Escuchar conexiones
    if (listen(server_sock, 10) < 0) {
        perror("[-] Error en listen");
        close(server_sock);
        pthread_exit(NULL);
    }

    printf("[+] Servidor '%s' listo en puerto %d\n", alias, MAIN_PORT);

    while (1) {
        // Esperar turno Round Robin
        pthread_mutex_lock(&turn_mutex);
        while (my_id != current_turn) {
            printf("[*] Servidor '%s' esperando turno (turno actual: %s)\n", alias, alias_list[current_turn]);
            pthread_cond_wait(&turn_cond, &turn_mutex);
        }
        pthread_mutex_unlock(&turn_mutex);

        printf("[*] Turno del servidor %s iniciado...\n", alias);

        // Iniciar temporizador
        signal(SIGALRM, SIG_IGN);
        stop = 0; 
        alarm(QUANTUM);

        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);
        int *client_sock = malloc(sizeof(int));

        // Esperar hasta que llegue un cliente o se acabe el quantum
        fd_set read_fds;
        struct timeval timeout;
        timeout.tv_sec = QUANTUM;
        timeout.tv_usec = 0;
        FD_ZERO(&read_fds);
        FD_SET(server_sock, &read_fds);

        int activity = select(server_sock + 1, &read_fds, NULL, NULL, &timeout);
        if(activity < 0) {
            perror("[-] Error en select");
        } else if (activity == 0) {
            printf("[*] Quantum de %d segundos terminado para servidor '%s'\n", QUANTUM, alias);
        } else 
        {
            *client_sock = accept(server_sock, (struct sockaddr *)&cli_addr, &cli_len);
            if(*client_sock >= 0)
            {
                printf("[+] Servidor '%s' aceptó conexión de %s:%d\n", alias,
                       inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
                pthread_t tid;
                pthread_create(&tid, NULL, handle_client, client_sock);
                pthread_detach(tid);
            }
            if (*client_sock < 0) {
                perror("[-] Error en accept");
                free(client_sock);
            }
        }
        alarm(0); 
        // Pasar turno al siguiente servidor
        pthread_mutex_lock(&turn_mutex);
        current_turn = (current_turn + 1) % num_alias;
        printf("[*] Servidor '%s' cediendo turno a %s\n", alias, alias_list[current_turn]);
        pthread_cond_broadcast(&turn_cond);
        pthread_mutex_unlock(&turn_mutex);

        printf("[+] Servidor '%s' terminó su turno. Siguiente: %s\n",
               alias, alias_list[current_turn]);
    }
    close(server_sock);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Uso: %s <alias1> <alias2> ... <aliasN>\n", argv[0]);
        return 1;
    }

    num_alias = argc - 1;
    alias_list = &argv[1];
    pthread_t threads[num_alias];
    int ids[num_alias];

    // Crear hilo para cada alias
    for (int i = 0; i < num_alias; i++) {
        ids[i] = i;
        if (pthread_create(&threads[i], NULL, server_thread, &ids[i]) != 0) {
            perror("[-] Error al crear hilo de servidor");
            return 1;
        }
        sleep(1); 
    }

    for (int i = 0; i < num_alias; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}

