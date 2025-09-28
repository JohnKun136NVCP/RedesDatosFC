#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <time.h>
#include <pthread.h> 

#define DEFAULT_PORT 49200
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

pthread_mutex_t turn_mutex;      
pthread_cond_t turn_cond;       
int current_turn = 0;           
int is_receiving = 0;            
int num_servers;                 

typedef struct {
    int server_id;
    int sockfd;
    char *hostname;
} server_thread_args_t;

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

    struct stat st = {0};
    if (stat(hostname, &st) == -1) {
        mkdir(hostname, 0755);
    }

    int filename_len;
    if (recv(new_socket, &filename_len, sizeof(filename_len), 0) <= 0) {
        close(new_socket);
        return;
    }

    char filename[filename_len + 1];
    if (recv(new_socket, filename, filename_len, 0) <= 0) {
        close(new_socket);
        return;
    }
    filename[filename_len] = '\0';

    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s", hostname, filename);

    FILE *file = fopen(filepath, "wb");
    if (!file) {
        log_event(hostname, filename, "ERROR_AL_ABRIR_ARCHIVO");
        close(new_socket);
        return;
    }

    printf("--> [%s] está RECIBIENDO el archivo '%s'\n", hostname, filename);
    log_event(hostname, filename, "RECEPCION_INICIADA");

    while ((bytes_received = recv(new_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes_received, file);
    }

    fclose(file);
    close(new_socket);
    log_event(hostname, filename, "RECIBIDO_EXITOSAMENTE");
    printf("--> [%s] TERMINÓ de recibir '%s'\n", hostname, filename);
}

void *server_routine(void *arg) {
    server_thread_args_t *args = (server_thread_args_t *)arg;
    int my_id = args->server_id;
    int my_sock = args->sockfd;
    char *my_hostname = args->hostname;

    while (1) {
        pthread_mutex_lock(&turn_mutex);

        while (current_turn != my_id || is_receiving) {
            pthread_cond_wait(&turn_cond, &turn_mutex);
        }

        printf("Turno del servidor %d (%s)\n", my_id, my_hostname);
        
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(my_sock, &readfds);

        struct timeval timeout;
        timeout.tv_sec = 0; 
        timeout.tv_usec = 1000; 

        int activity = select(my_sock + 1, &readfds, NULL, NULL, &timeout);

        if (activity > 0) { 
            is_receiving = 1; 
            pthread_mutex_unlock(&turn_mutex); 

            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int new_socket = accept(my_sock, (struct sockaddr *)&client_addr, &client_len);
            if (new_socket >= 0) {
                handle_client(new_socket, my_hostname);
            }

            pthread_mutex_lock(&turn_mutex); 
            is_receiving = 0; 
            current_turn = (current_turn + 1) % num_servers; 
        } else {
            printf("Servidor %d (%s) cede su turno (sin clientes)\n", my_id, my_hostname);
            current_turn = (current_turn + 1) % num_servers;
        }

        pthread_cond_broadcast(&turn_cond);
        pthread_mutex_unlock(&turn_mutex);

        sleep(1);
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <host1> [host2 ...]\n", argv[0]);
        exit(1);
    }

    num_servers = argc - 1;
    pthread_t threads[num_servers];
    server_thread_args_t thread_args[num_servers];

    pthread_mutex_init(&turn_mutex, NULL);
    pthread_cond_init(&turn_cond, NULL);

    for (int i = 0; i < num_servers; i++) {
        struct hostent *he;
        struct sockaddr_in server_addr;

        if ((he = gethostbyname(argv[i + 1])) == NULL) {
            perror("gethostbyname"); exit(1);
        }

        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("socket"); exit(1);
        }

        int opt = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            perror("setsockopt"); exit(1);
        }

        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(DEFAULT_PORT);
        server_addr.sin_addr = *((struct in_addr *)he->h_addr);

        if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("bind"); exit(1);
        }

        if (listen(sockfd, MAX_CLIENTS) < 0) {
            perror("listen"); exit(1);
        }

        printf("Servidor %d escuchando en %s:%d\n", i, argv[i + 1], DEFAULT_PORT);

        thread_args[i].server_id = i;
        thread_args[i].sockfd = sockfd;
        thread_args[i].hostname = argv[i + 1];

        if (pthread_create(&threads[i], NULL, server_routine, &thread_args[i]) != 0) {
            perror("pthread_create");
            exit(1);
        }
    }

    for (int i = 0; i < num_servers; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&turn_mutex);
    pthread_cond_destroy(&turn_cond);

    return 0;
}

