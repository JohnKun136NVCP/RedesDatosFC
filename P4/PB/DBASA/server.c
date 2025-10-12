#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>

#define BUFFER_SIZE 1024
#define BASE_PORT 49200

typedef struct {
    char alias[32];
    char ip[INET_ADDRSTRLEN];
} AliasInfo;

//Cifrado Cesar
void encryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c))
            text[i] = ((c - 'A' + shift) % 26) + 'A';
        else if (islower(c))
            text[i] = ((c - 'a' + shift) % 26) + 'a';
    }
}

//Logs
void send_log(int log_sock, const char *estado, const char *alias) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

    char log_msg[128];
    snprintf(log_msg, sizeof(log_msg), "[%s][%s] %s\n", timestamp, alias, estado);

    printf("%s", log_msg);
    fflush(stdout);

    if (log_sock > 0)
        send(log_sock, log_msg, strlen(log_msg), 0);
}

//Crear socket en puerto
int create_socket_on_port(const char *ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[-] Error creando socket");
        exit(1);
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[-] Error en bind");
        close(sock);
        return -1;
    }

    if (listen(sock, 5) < 0) {
        perror("[-] Error en listen");
        close(sock);
        return -1;
    }

    return sock;
}

//Buscar puerto libre
int find_free_port(const char *ip) {
    int port = BASE_PORT + 1;
    while (port < 50000) {
        int test_sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, ip, &addr.sin_addr);
        int result = bind(test_sock, (struct sockaddr *)&addr, sizeof(addr));
        close(test_sock);
        if (result == 0)
            return port;
        port++;
    }
    return -1;
}

//Resolver IP
char *resolve_alias_ip(const char *alias) {
    struct hostent *he = gethostbyname(alias);
    if (he == NULL) {
        herror("[-] No se pudo resolver el alias");
        exit(1);
    }
    static char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, he->h_addr_list[0], ip, sizeof(ip));
    return ip;
}

//Hilo del servidor para un alias (espero sí estar utilizando concurrencia :c) 
void *server_thread(void *arg) {
    AliasInfo *info = (AliasInfo *)arg;
    const char *alias = info->alias;
    const char *ip = info->ip;

    mkdir(alias, 0777);
    printf("[+] Iniciando servidor para %s (%s)\n", alias, ip);

    int main_sock = create_socket_on_port(ip, BASE_PORT);
    if (main_sock < 0) {
        fprintf(stderr, "[-] No se pudo abrir socket en %s:%d\n", ip, BASE_PORT);
        pthread_exit(NULL);
    }

    printf("[+] %s escuchando en %s:%d\n", alias, ip, BASE_PORT);

    int log_sock = -1;
    struct sockaddr_in client_addr;
    socklen_t addr_size = sizeof(client_addr);

    send_log(log_sock, "en espera de conexión", alias);

    while (1) {
        int client = accept(main_sock, (struct sockaddr *)&client_addr, &addr_size);
        if (client < 0) {
            perror("[-] Error en accept");
            continue;
        }

        char buffer[BUFFER_SIZE];
        int bytes = recv(client, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            close(client);
            continue;
        }

        buffer[bytes] = '\0';

        if (strncmp(buffer, "ASSIGN", 6) == 0) {
            int new_port = find_free_port(ip);
            if (new_port == -1) {
                printf("[-] No se encontró puerto libre\n");
                close(client);
                continue;
            }

            char msg[64];
            snprintf(msg, sizeof(msg), "ASIGNADO %d\n", new_port);
            send(client, msg, strlen(msg), 0);
            printf("[+] %s asignó puerto %d\n", alias, new_port);

            int ctrl_sock = create_socket_on_port(ip, new_port);
            int ctrl_client = accept(ctrl_sock, (struct sockaddr *)&client_addr, &addr_size);
            if (ctrl_client >= 0) {
                log_sock = ctrl_client;
                send_log(log_sock, "en espera de conexión", alias);
                printf("[+] %s estableció conexión de control (%d)\n", alias, new_port);
            }
            close(ctrl_sock);
            close(client);
        }

        else {
            send_log(log_sock, "recibiendo información", alias);
            encryptCaesar(buffer, 3);

            char filepath[256];
            snprintf(filepath, sizeof(filepath), "%s/mensaje_encriptado.txt", alias);
            FILE *out = fopen(filepath, "w");
            if (out) {
                fprintf(out, "%s\n", buffer);
                fclose(out);
                printf("[+] %s guardó mensaje encriptado en %s\n", alias, filepath);
            }

            send_log(log_sock, "mandando información", alias);
            send(client, "Archivo recibido y encriptado.\n", 31, 0);
            send_log(log_sock, "en espera de conexión", alias);
            close(client);
        }
    }

    close(main_sock);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Uso: %s <alias1> [alias2] [alias3] ...\n", argv[0]);
        exit(1);
    }

    int num_aliases = argc - 1;
    pthread_t threads[num_aliases];
    AliasInfo infos[num_aliases];

    for (int i = 0; i < num_aliases; i++) {
        strcpy(infos[i].alias, argv[i + 1]);
        strcpy(infos[i].ip, resolve_alias_ip(argv[i + 1]));

        if (pthread_create(&threads[i], NULL, server_thread, &infos[i]) != 0) {
            perror("[-] Error creando hilo de servidor");
        }

        sleep(1);
    }

    for (int i = 0; i < num_aliases; i++)
        pthread_join(threads[i], NULL);

    return 0;
}
