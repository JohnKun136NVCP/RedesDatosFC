#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <netdb.h>

#define MAIN_PORT 49200
#define BUFFER_SIZE 1024
#define MAX_QUEUE 10
#define QUANTUM 10

typedef struct
{
    int socket;
    struct sockaddr_in addr;
    FILE *fp;
    char filename[256];
    bool sending;
} Client;

Client queue[MAX_QUEUE];
int front = 0, rear = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

// Agregar cliente a la cola
void enqueue(Client c)
{
    int next = (rear + 1) % MAX_QUEUE;
    if (next == front)
    {
        printf("Cola llena");
        return;
    }
    queue[rear] = c;
    rear = next;
}

// Eliminar cliente de la cola
Client dequeue()
{
    Client c = queue[front];
    front = (front + 1) % MAX_QUEUE;
    return c;
}

bool isEmpty()
{
    return front == rear;
}

// Hilo planificador
void *scheduler(void *arg)
{
    char buffer[BUFFER_SIZE];
    while (1)
    {
        if (!isEmpty())
        {
            // Hay clientes en la cola
            pthread_mutex_lock(&lock);
            Client c = dequeue();
            pthread_mutex_unlock(&lock);

            printf("[+] Atendiendo cliente en socket %d\n", c.socket);

            time_t start = time(NULL);
            // Atender al cliente por un quantum de tiempo
            while (difftime(time(NULL), start) < QUANTUM)
            {
                int bytes = recv(c.socket, buffer, BUFFER_SIZE, MSG_DONTWAIT);
                // Manejar datos recibidos
                if (bytes > 0)
                {
                    buffer[bytes] = '\0';
                    // Si no está enviando, el primer mensaje es el nombre del archivo
                    if (!c.sending)
                    {
                        strncpy(c.filename, buffer, sizeof(c.filename));
                        c.fp = fopen(c.filename, "wb");
                        // Manejar error al abrir archivo
                        if (!c.fp)
                        {
                            perror("[-] Error al abrir el archivo");
                            close(c.socket);
                            break;
                        }
                        c.sending = true;
                        printf("[+] Recibiendo archivo %s\n", c.filename);
                    }
                    // Si está enviando, escribir datos en el archivo
                    else if (strcmp(buffer, "EOF") == 0)
                    {
                        printf("[+] Archivo %s enviado\n", c.filename);
                        fclose(c.fp);
                        c.fp = NULL; 
                        c.sending = false;
                        close(c.socket);
                        break;
                    }
                    // Si el archivo está abierto, escribir los datos recibidos
                    else
                    {
                        fwrite(buffer, 1, bytes, c.fp);
                    }
                }
                // Manejar desconexión del cliente
                else if (bytes == 0)
                {
                    printf("[-] Cliente desconectado\n");
                    // Cerrar archivo si está abierto
                    if (c.fp)
                    {
                        fclose(c.fp);
                    }
                    close(c.socket);
                    break;
                }
                usleep(100000);
            }

            // Si el cliente aún está enviando, reinsertarlo en la cola
            if (recv(c.socket, buffer, BUFFER_SIZE, MSG_PEEK | MSG_DONTWAIT) > 0)
            {
                pthread_mutex_lock(&lock);
                enqueue(c);
                pthread_mutex_unlock(&lock);
                printf("[*] Cliente %s reinsertado en la cola\n", c.filename);
            }
        }
        else
        {
            sleep(1);
        }
    }
    return NULL;
}

int main()
{
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(client_addr);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1)
    {
        perror("[-] Error al crear socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(MAIN_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("[-] Error al bind");
        close(server_sock);
        return 1;
    }

    if (listen(server_sock, 10) < 0)
    {
        perror("[-] Error al escuchar");
        close(server_sock);
        exit(1);
    }

    printf("[+] Servidor escuchando en puerto %d...\n", MAIN_PORT);

    pthread_t sched_thread;
    pthread_create(&sched_thread, NULL, scheduler, NULL);

    while (1)
    {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
        if (client_sock < 0)
        {
            perror("[-] Error en accept");
            continue;
        }

        Client c;
        c.socket = client_sock;
        c.addr = server_addr;
        c.fp = NULL;
        c.sending = false;

        pthread_mutex_lock(&lock);
        enqueue(c);
        pthread_mutex_unlock(&lock);

        printf("[+] Nuevo cliente conectado en socket %d\n", client_sock);
    }

    close(server_sock);
    return 0;
}