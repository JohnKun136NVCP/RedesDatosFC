
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

#define MAIN_PORT 49200  
#define BUFFER_SIZE 1024 

char **alias_list;
int num_alias;

// Función para obtener fecha y hora
void get_datetime(char *buffer, size_t size)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", t);
}

void *handle_client(void *arg)
{
    int client_sock = *(int *)arg;
    free(arg);
    char buffer[BUFFER_SIZE];
    int bytes;

    // Recibir mensaje
    while ((bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0)) > 0)
    {
        buffer[bytes] = '\0';
        char datetime[64];
        get_datetime(datetime, sizeof(datetime));
        printf("[%s] Mensaje recibido: %s\n", datetime, buffer);
        fflush(stdout);
    }

    close(client_sock);
    pthread_exit(NULL);
}

void *server_thread(void *arg)
{
    char *alias = (char *)arg;

    struct addrinfo hints, *res;
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", MAIN_PORT);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    // Obtener información del servidor
    if (getaddrinfo(alias, port_str, &hints, &res) != 0)
    {
        perror("[-] Error al resolver el host");
        pthread_exit(NULL);
    }

    // Crear socket
    int server_sock = socket(res -> ai_family, res->ai_socktype, res->ai_protocol);
    if (server_sock < 0) {
        perror("[-] Error al crear el socket del servidor");
        freeaddrinfo(res);
        pthread_exit(NULL);
    }

    // Configurar opciones del socket
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Enlazar el socket
    if (bind(server_sock, res -> ai_addr, res -> ai_addrlen) < 0) {
        perror("[-] Error en bind del servidor");
        close(server_sock);
        freeaddrinfo(res);
        pthread_exit(NULL);
    }

    // Escuchar conexiones entrantes
    freeaddrinfo(res);
    if (listen(server_sock, 5) < 0) {
        perror("[-] Error en listen del servidor");
        close(server_sock);
        pthread_exit(NULL);
    }

    printf("[+] Servidor '%s' escuchando en puerto %d...\n", alias, MAIN_PORT);

    while (1)
    {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);
        int *client_sock = malloc(sizeof(int));

        // Aceptar conexión entrante
        if ((*client_sock = accept(server_sock, (struct sockaddr *)&cli_addr, &cli_len)) < 0)
        {
            perror("[-] Error al aceptar conexión");
            free(client_sock);
            continue;
        }

        pthread_t tid;
        // Crear hilo para manejar al cliente
        if (pthread_create(&tid, NULL, handle_client, client_sock) != 0)
        {
            perror("[-] Error al crear hilo para el cliente");
            close(*client_sock);
            free(client_sock);
            continue;
        }
        pthread_detach(tid);
    }

    close(server_sock);
    pthread_exit(NULL);
}

void send_to_all(const char *sender, const char *message)
{
    for (int i = 0; i < num_alias; i++)
    {
        if (strcmp(alias_list[i], sender) == 0)
        {
            continue;
        }

        // Resolver host o IP
        struct addrinfo hints, *res;
        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%d", MAIN_PORT);

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo(alias_list[i], port_str, &hints, &res) != 0)
        {
            perror("[-] Error al resolver el host");
            continue;
        }

        // Crear socket
        int client_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (client_sock < 0)
        {
            perror("[-] Error al crear el socket");
            freeaddrinfo(res);
            continue;
        }

        // Conectar al servidor
        if (connect(client_sock, res->ai_addr, res->ai_addrlen) < 0)
        {
            perror("[-] Error al conectar al servidor");
            close(client_sock);
            freeaddrinfo(res);
            continue;
        }

        char full_msg[BUFFER_SIZE];
        snprintf(full_msg, sizeof(full_msg), "%s -> %s : %s", sender, alias_list[i], message);

        // Enviar mensaje
        if (send(client_sock, full_msg, strlen(full_msg), 0) < 0)
        {
            perror("[-] Error al enviar mensaje");
            close(client_sock);
            freeaddrinfo(res);
            continue;
        }

        close(client_sock);
        freeaddrinfo(res);
        sleep(1);
    }
}

void *client_thread(void *arg)
{
    char *alias = (char *)arg; 
    sleep(2); 
    send_to_all(alias, "Hola a todos desde el servidor!");
    return NULL; 
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Uso: %s <alias1> <alias2> ... <aliasN>\n", argv[0]);
        return 1;
    }

    num_alias = argc - 1;
    alias_list = &argv[1];

    pthread_t client_threads[num_alias];
    pthread_t server_threads[num_alias];

    if (!client_threads || !server_threads)
    {
        perror("[-] Error al asignar memoria para hilos");
        return 1;
    }

    for (int i = 0; i < num_alias; i++)
    {
        if (pthread_create(&server_threads[i], NULL, server_thread, alias_list[i]) != 0)
        {
            perror("[-] Error al crear hilo del servidor");
            return 1;
        }
    }

    for (int i = 0; i < num_alias; i++)
    {
        if (pthread_create(&client_threads[i], NULL, client_thread, alias_list[i]) != 0)
        {
            perror("[-] Error al crear hilo de envío");
            return 1;
        }
    }

    for (int i = 0; i < num_alias; i++)
    {
        pthread_join(server_threads[i], NULL);
        pthread_join(client_threads[i], NULL);
    }
    return 0;
}
 