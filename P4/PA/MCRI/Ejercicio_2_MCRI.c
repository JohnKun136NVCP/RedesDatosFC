
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <netdb.h>
#include <ctype.h>
#include <stdbool.h>

#define MAIN_PORT 49200  
#define BUFFER_SIZE 1024 

FILE *log_file;

// Función para obtener fecha y hora
void get_datetime(char *buffer, size_t size)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", t);
}

// Función para manejar cada cliente
void *handle_client(void *arg)
{
    int dynamic_sock = *(int *)arg;
    free(arg);

    int client_sock;
    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    char buffer[BUFFER_SIZE];
    char datetime[64];
    char host[NI_MAXHOST];
    int bytes;

    // Obtener información del cliente
    if (getnameinfo((struct sockaddr *)&cli_addr, cli_len, host, sizeof(host), NULL, 0, 0) == 0)
    {
        get_datetime(datetime, sizeof(datetime));
        fprintf(log_file, "[%s] Conexión desde %s\n", datetime, host);
        fflush(log_file);
    }
    else
    {
        inet_ntop(AF_INET, &cli_addr.sin_addr, host, sizeof(host));
        get_datetime(datetime, sizeof(datetime));
        fprintf(log_file, "[%s] Conexión desde %s\n", datetime, host);
        fflush(log_file);
    }

    // Recibir nombre del archivo
    bytes = recv(dynamic_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0)
    {
        close(dynamic_sock);
        return NULL;
    }
    buffer[bytes] = '\0';

    char filename[256];
    snprintf(filename, sizeof(filename), "%s", buffer);

    // Loguear la recepción del archivo
    get_datetime(datetime, sizeof(datetime));
    fprintf(log_file, "[%s] Servidor recibiendo archivo %s\n", datetime, buffer);
    fflush(log_file);

    // Crear y abrir el archivo para escribir
    FILE *fp = fopen(filename, "wb");
    if (!fp)
    {
        perror("[-] Error al crear archivo");
        close(dynamic_sock);
        pthread_exit(NULL);
    }

    // Recibir datos
    while ((bytes = recv(dynamic_sock, buffer, sizeof(buffer), 0)) > 0)
    {
        fwrite(buffer, 1, bytes, fp);
    }
    fclose(fp);

    get_datetime(datetime, sizeof(datetime));
    fprintf(log_file, "[%s] Se ha recibido el archivo %s.\n", datetime, filename);
    fflush(log_file);

    snprintf(buffer, sizeof(buffer), "Servidor recibió el archivo %s correctamente.", filename);
    send(dynamic_sock, buffer, strlen(buffer), 0);

    get_datetime(datetime, sizeof(datetime));
    fprintf(log_file, "[%s] Comunicación con cliente finalizada. Servidor en espera.\n", datetime);
    fflush(log_file);

    close(dynamic_sock);
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    int server_sock, client_sock, dynamic_fd, dynamic_sock;
    struct sockaddr_in serv_addr, dynamic_addr;
    socklen_t addr_size = sizeof(serv_addr);
    char buffer[BUFFER_SIZE];
    char datetime[64];
    int opt = 1;

    log_file = fopen("server_log.txt", "a");
    if (!log_file)
    {
        perror("[-] No se pudo abrir el archivo de log");
        return 1;
    }

    // Crear socket
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[-] Error al crear el socket");
        return 1;
    }

    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(MAIN_PORT);

    // Enlazar el socket a la dirección y puerto especificados
    if (bind(server_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("[-] Error en bind");
        close(server_sock);
        return -1;
    }

    // Escuchar conexiones entrantes
    if (listen(server_sock, 5) < 0)
    {
        perror("[-] Error en listen");
        close(server_sock);
        return -1;
    }

    printf("[+] Server escuchando puerto %d...\n", MAIN_PORT);

    while (1)
    {
        if ((client_sock = accept(server_sock, (struct sockaddr *)&serv_addr, &addr_size)) < 0)
        {
            perror("[-] Error al aceptar");
            continue;
        }

        printf("[+] Cliente conectado al puerto %d \n", MAIN_PORT);

        // Asignar puerto dinámico
        int dynamic_port = 49201 + rand() % 1000; // Puerto entre 49201 y 50200
        if ((dynamic_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            perror("[-] Error al crear el socket del puerto dinámico");
            close(client_sock);
            continue;
        }

        struct sockaddr_in dynamic_addr;
        memset(&dynamic_addr, 0, sizeof(dynamic_addr));
        dynamic_addr.sin_family = AF_INET;
        dynamic_addr.sin_addr.s_addr = INADDR_ANY;
        dynamic_addr.sin_port = htons(dynamic_port);

        // Enlazar el socket del puerto dinámico
        if (bind(dynamic_fd, (struct sockaddr *)&dynamic_addr, sizeof(dynamic_addr)) < 0)
        {
            perror("[-] Error en bind del puerto dinámico");
            close(client_sock);
            close(dynamic_fd);
            continue;
        }

        // Esperar nueva conexión
        if (listen(dynamic_fd, 1) < 0)
        {
            perror("[-] Error en listen del puerto dinámico");
            close(client_sock);
            close(dynamic_fd);
            continue;
        }

        // Enviar puerto dinámico al cliente
        int net_port = htonl(dynamic_port);
        if (send(client_sock, &net_port, sizeof(net_port), 0) == -1)
        {
            perror("[-] Error al enviar el puerto dinámico");
            close(client_sock);
            close(dynamic_fd);
            continue;
        }
        close(client_sock);

        // Aceptar conexión en el puerto dinámico
        char datetime[64];
        get_datetime(datetime, sizeof(datetime));
        fprintf(log_file, "[%s] Puerto dinámico %d asignado\n", datetime, dynamic_port);
        fflush(log_file);

        // Esperar conexión en el puerto dinámico
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);
        if ((dynamic_sock = accept(dynamic_fd, (struct sockaddr *)&cli_addr, &cli_len)) < 0)
        {
            perror("[-] Error al aceptar en el puerto dinámico");
            close(dynamic_fd);
            continue;
        }

        // Preparar y enviar estado
        get_datetime(datetime, sizeof(datetime));
        snprintf(buffer, sizeof(buffer), "Archivo enviado Fecha y hora: %s\n", datetime);
        if (send(dynamic_sock, buffer, strlen(buffer), 0) == -1)
        {
            perror("[-] Error al enviar el estado");
        }

        // Manejar cliente en un hilo separado
        int *arg = malloc(sizeof(int));
        *arg = dynamic_sock;
        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, arg);
        pthread_detach(tid);
    }
    fclose(log_file);
    close(server_sock);
    return 0;
}

