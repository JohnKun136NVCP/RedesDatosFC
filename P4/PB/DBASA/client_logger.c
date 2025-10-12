#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define BASE_PORT 49200
#define BUFFER_SIZE 1024

// Estructura de datos para cada conexión
typedef struct {
    char alias[32];
    char ip[INET_ADDRSTRLEN];
} LoggerTarget;

//Resolver IP desde alias
char *get_ip_from_alias(const char *alias) {
    struct hostent *he = gethostbyname(alias);
    if (he == NULL) {
        herror("[-] Error obteniendo IP del alias");
        exit(1);
    }
    static char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, he->h_addr_list[0], ip, sizeof(ip));
    return ip;
}

//Conectar a IP:puerto
int connect_to(const char *ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[-] Error creando socket");
        return -1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "[-] No se pudo conectar a %s:%d\n", ip, port);
        close(sock);
        return -1;
    }

    return sock;
}

//Hilo que maneja logs de un alias
void *handle_logs(void *arg) {
    LoggerTarget *target = (LoggerTarget *)arg;
    const char *alias = target->alias;
    const char *ip = target->ip;

    printf("[+] Conectando al servidor %s (%s)...\n", alias, ip);

    int main_sock = connect_to(ip, BASE_PORT);
    if (main_sock < 0) pthread_exit(NULL);

    send(main_sock, "ASSIGN\n", 7, 0);

    char response[BUFFER_SIZE];
    int bytes = recv(main_sock, response, sizeof(response) - 1, 0);
    if (bytes <= 0) {
        fprintf(stderr, "[-] No se recibió respuesta del servidor %s\n", alias);
        close(main_sock);
        pthread_exit(NULL);
    }

    response[bytes] = '\0';
    int new_port = 0;
    sscanf(response, "ASIGNADO %d", &new_port);
    close(main_sock);

    if (new_port == 0) {
        fprintf(stderr, "[-] No se obtuvo puerto válido de %s\n", alias);
        pthread_exit(NULL);
    }

    printf("[+] %s asignó puerto de control: %d\n", alias, new_port);

    int log_sock = connect_to(ip, new_port);
    if (log_sock < 0) pthread_exit(NULL);

    char filename[128];
    snprintf(filename, sizeof(filename), "logs_%s.txt", alias);
    FILE *log_file = fopen(filename, "a");
    if (!log_file) {
        perror("[-] No se pudo abrir archivo de logs");
        close(log_sock);
        pthread_exit(NULL);
    }

    printf("[+] Escuchando logs de %s -> guardando en %s\n\n", alias, filename);

    while (1) {
        char buffer[BUFFER_SIZE];
        int n = recv(log_sock, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) {
            printf("[-] Conexión cerrada con %s\n", alias);
            break;
        }
        buffer[n] = '\0';

        printf("[%s] %s", alias, buffer);
        fflush(stdout);

        fprintf(log_file, "[%s] %s", alias, buffer);
        fflush(log_file);
    }

    fclose(log_file);
    close(log_sock);
    pthread_exit(NULL);
}

//Genera lista de alias remotos
int get_other_aliases(const char *local_alias, const char *all_aliases[], const int total, const char *result[]) {
    int count = 0;
    for (int i = 0; i < total; i++) {
        if (strcmp(local_alias, all_aliases[i]) != 0) {
            result[count++] = all_aliases[i];
        }
    }
    return count;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <alias_local>\n", argv[0]);
        printf("Ejemplo: %s s02\n", argv[0]);
        exit(1);
    }

    const char *local_alias = argv[1];
    printf("[+] Logger iniciado desde %s\n", local_alias);

    const char *all_aliases[] = {"s01", "s02", "s03", "s04"};
    const int total_aliases = sizeof(all_aliases) / sizeof(all_aliases[0]);

    const char *targets[total_aliases - 1];
    int num_targets = get_other_aliases(local_alias, all_aliases, total_aliases, targets);

    pthread_t threads[num_targets];
    LoggerTarget logger_targets[num_targets];

    for (int i = 0; i < num_targets; i++) {
        strcpy(logger_targets[i].alias, targets[i]);
        strcpy(logger_targets[i].ip, get_ip_from_alias(targets[i]));

        if (pthread_create(&threads[i], NULL, handle_logs, &logger_targets[i]) != 0) {
            perror("[-] Error creando hilo de logger");
        }

        sleep(1);
    }

    for (int i = 0; i < num_targets; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("[+] Logger finalizado.\n");
    return 0;
}
