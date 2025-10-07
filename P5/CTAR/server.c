#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/mman.h>
#include <time.h>

//gcc server.c -o server -lpthread
  
#define BUFFER_SIZE 1024
#define server_port 49200 // Puerto base 
#define QUANTUM_TIME 15

/*
    Estructura para memoria compartida. Con esto nos aseguramos que solo un servidor
    esté recibiendo archivos a la vez y que cada servidor espere su turno. Además notifica
    a los demas servidores cuando es su turno.
*/
typedef struct {
    int current_server;
    int receiving_server;
    bool server_busy;
    time_t turn_start_time;
    //Mnejamos la sincronización donde los hilos de cada servidor esperan su turno
    pthread_mutex_t mutex;
    pthread_cond_t turn_cond;
} shared_memory_t;

/*
    Estructura para cola de conexiones. Cada servidor tiene su propia cola donde se almacenan
    las conexiones entrantes mientras espera su turno.
*/
typedef struct connection_node {
    int dynamic_client;
    int dynamic_sock;
    char target_server[32];
    struct connection_node* next;
} connection_node_t;

shared_memory_t *shared_mem;
char *server_names[4];
// Inicializamos una cola para cada servidor donde se almacenan las conexiones entrantes
connection_node_t* connection_queues[4] = {NULL}; 
pthread_mutex_t queue_mutexes[4];

/*
    Función para guardar archivo en el directorio del servidor
*/
void saveFile(const char *server_name, const char *filename, const char *content) {
    char file_path[256];
    char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        home_dir = "/home";
        printf("Warning: HOME environment variable not set, using %s\n", home_dir);
    }
    
    snprintf(file_path, sizeof(file_path), "%s/%s/%s", home_dir, server_name, filename);
    FILE *file = fopen(file_path, "w");
    if (file) {
        fprintf(file, "%s", content);
        fclose(file);
    }
}

/*
    Función para verificar si el tiempo del servidor ha expirado
*/
bool quantumExpired(time_t start_time) {
    return (time(NULL) - start_time) >= QUANTUM_TIME;
}

/*
    Funcion que agrega una conexión a la cola del servidor correspondiente
*/
void addQueue(const char* target_server, int dynamic_client, int dynamic_sock) {
    connection_node_t* new_node = malloc(sizeof(connection_node_t));
    new_node->dynamic_client = dynamic_client;
    new_node->dynamic_sock = dynamic_sock;
    strncpy(new_node->target_server, target_server, sizeof(new_node->target_server) - 1);
    new_node->target_server[sizeof(new_node->target_server) - 1] = '\0';
    new_node->next = NULL;
    
    int server_index = -1;
    for (int i = 0; i < 4; i++) {
        if (strcmp(target_server, server_names[i]) == 0) {
            server_index = i;
            break;
        }
    }
    
    if (server_index == -1) {
        free(new_node);
        return;
    }
    
    pthread_mutex_lock(&queue_mutexes[server_index]);
    
    if (connection_queues[server_index] == NULL) {
        connection_queues[server_index] = new_node;
    } else {
        connection_node_t* current = connection_queues[server_index];
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_node;
    }
    
    pthread_mutex_unlock(&queue_mutexes[server_index]);
}

/*
    Función que obtiene la siguiente conexión de la cola
*/
connection_node_t* getNextConnection(int server_index) {
    pthread_mutex_lock(&queue_mutexes[server_index]);
    
    if (connection_queues[server_index] == NULL) {
        pthread_mutex_unlock(&queue_mutexes[server_index]);
        return NULL;
    }
    
    connection_node_t* node = connection_queues[server_index];
    connection_queues[server_index] = node->next;
    
    pthread_mutex_unlock(&queue_mutexes[server_index]);
    return node;
}

/*
    Función que procesa la conexión donde recibe el archivo y lo guarda si es el servidor correcto
*/
void processConnection(int dynamic_client, int dynamic_sock, const char* target_server) {
    char buffer[BUFFER_SIZE] = {0};
    char file_content[BUFFER_SIZE] = {0};
    char filename[256];

    while(1){
        int bytes = recv(dynamic_client, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            break;
        }else{
            buffer[bytes] = '\0';
            
            char alias[32];
            if (sscanf(buffer, "%31[^|]|%255[^|]|%[^\n]", alias, filename, file_content) == 3) {
                if (strcmp(alias, target_server) == 0) {
                    saveFile(alias, filename, file_content);
                    char *msg = "File received successfully";
                    send(dynamic_client, msg, strlen(msg), 0);
                    printf("[SERVER %s] File %s received\n", alias, filename);
                } else {
                    char *msg = "REJECTED - Wrong server";
                    send(dynamic_client, msg, strlen(msg), 0);
                    printf("[SERVER %s] Rejected file for %s\n", target_server, alias);
                }
            } else {
                char *msg = "REJECTED";
                send(dynamic_client, msg, strlen(msg), 0);
            }

            memset(buffer, 0, sizeof(buffer));
            memset(file_content, 0, sizeof(file_content));
            memset(filename, 0, sizeof(filename));
        }
    }
    
    close(dynamic_client);
    close(dynamic_sock);
}

/*
    Función del hilo de cada servidor que espera su turno y procesa las conexiones en su cola usando Round Robin
*/
void* serverThread(void* arg) {
    int server_index = *(int*)arg;
    free(arg);
    bool first_waiting_printed = false;
    
    //Servidor simpre activo
    while (1) {
        
        pthread_mutex_lock(&shared_mem->mutex);
        // Los demas servidores esperan su turno
        while (shared_mem->current_server != server_index || shared_mem->server_busy) {
            if (!first_waiting_printed) {
                printf("[SERVER %s] Waiting for turn (current: %s)\n", 
                       server_names[server_index], server_names[shared_mem->current_server]);
                first_waiting_printed = true;
            }
            pthread_cond_wait(&shared_mem->turn_cond, &shared_mem->mutex);
        }
        
        //Iniciamos el turno del servidor actual y marcamos que está ocupado
        shared_mem->server_busy = true;
        shared_mem->receiving_server = server_index;
        shared_mem->turn_start_time = time(NULL);
        
        printf("\n[SERVER %s] Starting turn\n", server_names[server_index]);

        //Liberamos el mutex para que otros servidores puedan leer la memoria compartida
        pthread_mutex_unlock(&shared_mem->mutex);
        
        time_t start_time = time(NULL);
        bool processed_any = false;
        int files_processed = 0;
        
        //Procesamos conexiones hasta que expire el quantum. Nos aseguramos que cada servidor tenga su turno y no se quede esperando indefinidamente.
        while (!quantumExpired(start_time)) {
            connection_node_t* connection = getNextConnection(server_index);
            //Nos aseguramos que el servidor procese las conexiones en su cola, si se le acaba el tiempo y aun hay conexiones, debe esperar su siguiente turno
            // Si el tiempo se acaba mientras procesa una conexión, la termina y cede el turno
            if (connection != NULL) {
                processed_any = true;
                files_processed++;
                processConnection(connection->dynamic_client, connection->dynamic_sock, server_names[server_index]);
                free(connection);
            } else {
                if (processed_any) {
                    time_t remaining = QUANTUM_TIME - (time(NULL) - start_time);
                    if (remaining > 0) {
                        sleep(remaining);
                    }
                    break;
                } else {
                    sleep(1);
                }
            }
        }
        
        time_t time_used = time(NULL) - start_time;

        if (!processed_any) {
            printf("[SERVER %s] Quantum expired with no files to process\n", 
                   server_names[server_index]);
        } 
        
        //Finalizamos el turno y notificamos a los demas servidores
        pthread_mutex_lock(&shared_mem->mutex);
        
        shared_mem->server_busy = false;
        shared_mem->receiving_server = -1;
        shared_mem->current_server = (shared_mem->current_server + 1) % 4;
        
        printf("[SERVER %s] Turn finished\n", server_names[server_index]);
        
        pthread_cond_broadcast(&shared_mem->turn_cond);
        pthread_mutex_unlock(&shared_mem->mutex);
        usleep(1000); 
    }
    
    return NULL;
}

/*
    Funcion que maneja la conexión entrante, lee el alias y encola la conexión en la cola del servidor correspondiente
*/
void* connectionHand(void* arg) {
    int* sockets = (int*)arg;
    int dynamic_client = sockets[0];
    int dynamic_sock = sockets[1];
    free(arg);
    
    char buffer[BUFFER_SIZE] = {0};
    
    int bytes = recv(dynamic_client, buffer, sizeof(buffer) - 1, MSG_PEEK);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        
        char alias[32];
        char filename[256];
        char content[BUFFER_SIZE];
        
        if (sscanf(buffer, "%31[^|]|%255[^|]|%[^\n]", alias, filename, content) == 3) {
            addQueue(alias, dynamic_client, dynamic_sock);
        } else {
            close(dynamic_client);
            close(dynamic_sock);
        }
    } else {
        close(dynamic_client);
        close(dynamic_sock);
    }
    
    return NULL;
}

/*
    Función que maneja la expiración del quantum y cambia el turno si ningún servidor está ocupado
    En serverThread cada servidor ya liberan y ceden el turno, así que aquí solo nos aseguramos
    que si ningún servidor está ocupado y el quantum expiró, y el servidor actual no ha cedido el turno, lo haga.
*/
void* quantumAdmin(void* arg) {
    while (1) {
        sleep(5);      
        pthread_mutex_lock(&shared_mem->mutex);
        
        //Si el servidor no está recibiendo y el quantum expiró, cambiamos el turno
        if (!shared_mem->server_busy && quantumExpired(shared_mem->turn_start_time)) {
            shared_mem->current_server = (shared_mem->current_server + 1) % 4;
            shared_mem->turn_start_time = time(NULL);

            printf("[*] Switching to server: %s\n", server_names[shared_mem->current_server]);

            pthread_cond_broadcast(&shared_mem->turn_cond);
        }
        
        pthread_mutex_unlock(&shared_mem->mutex);
        usleep(1000); 
    }
    return NULL;
}

/*
    Función principal que inicializa el servidor, asignar puertos dinámicos a los clientes para recibir archivos y guardarlos en el directorio correspondiente (s01, s02, s03 o s04), 
    memoria compartida, hilos y espera conexiones entrantes
*/
int main(int argc, char *argv[]) {
    int port_s;
    int client_port; 
    struct sockaddr_in server_addr;
    int port_counter = 1;

    if (argc < 5) { 
        printf("Use: %s <s01> <s02> <s03> <s04>\n", argv[0]);
        return 1;
    }

    for (int i = 0; i < 4; i++) {
        server_names[i] = argv[i + 1];
        pthread_mutex_init(&queue_mutexes[i], NULL);
    }

    //Creamos el socket principal para el puerto base
    port_s = socket(AF_INET, SOCK_STREAM, 0);
    if (port_s < 0) {
        perror("[-] Error creating socket");
        return 1;
    }

    int opt = 1;
    //Permitimos que se vuelva a usar el puerto después de terminar la ejecución del programa
    if (setsockopt(port_s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR failed");
        return 1;
    }
    //Configuramos la dirección del servidor    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    //Asignamos el socket a la dirección y puerto especificados
    if (bind(port_s, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] Error binding");
        close(port_s);
        return 1;
    }

    //Escuchamos conexiones entrantes
    if (listen(port_s, 10) < 0) {
        perror("[-] Error on listen");
        close(port_s);
        return 1;
    }
    
    printf("[*] Round Robin initialized (quantum: %ds)\n", QUANTUM_TIME);
    printf("[*] Turn order: %s -> %s -> %s -> %s\n", server_names[0], server_names[1], server_names[2], server_names[3]);
    printf("[*] LISTENING on port %d...\n\n", server_port);

    shared_mem = mmap(NULL, sizeof(shared_memory_t), PROT_READ | PROT_WRITE, 
                     MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    shared_mem->current_server = 0;
    shared_mem->receiving_server = -1;
    shared_mem->server_busy = false;
    shared_mem->turn_start_time = time(NULL);
    pthread_mutex_init(&shared_mem->mutex, NULL);
    pthread_cond_init(&shared_mem->turn_cond, NULL);

    pthread_t serverThreads[4];
    for (int i = 0; i < 4; i++) {
        int* server_index = malloc(sizeof(int));
        *server_index = i;
        pthread_create(&serverThreads[i], NULL, serverThread, server_index);
        pthread_detach(serverThreads[i]);
    }

    pthread_t quantum_thread;
    pthread_create(&quantum_thread, NULL, quantumAdmin, NULL);
    pthread_detach(quantum_thread);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_size = sizeof(client_addr);
        client_port = accept(port_s, (struct sockaddr*)&client_addr, &addr_size);
        
        if (client_port < 0) {
            perror("Accept error");
            continue;
        }

        // Asignamos un puerto dinámico al cliente mayor al puerto base
        int dynamic_port = server_port + port_counter;
        port_counter++;
        
        //Enviamos el puerto dinámico al cliente
        char port_msg[64];
        snprintf(port_msg, sizeof(port_msg), "DYNAMIC_PORT|%d", dynamic_port);
        send(client_port, port_msg, strlen(port_msg), 0);
        close(client_port);

        //Creamos el socket para el puerto dinámico
        int dynamic_sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in dynamic_addr;
        
        dynamic_addr.sin_family = AF_INET;
        dynamic_addr.sin_port = htons(dynamic_port);
        dynamic_addr.sin_addr.s_addr = INADDR_ANY;
        
        // Permitimos que se vuelva a usar el puerto después de terminar la ejecución del programa
        int dyn_opt = 1;
        if (setsockopt(dynamic_sock, SOL_SOCKET, SO_REUSEADDR, &dyn_opt, sizeof(dyn_opt)) < 0) {
            perror("setsockopt SO_REUSEADDR failed on dynamic socket");
            close(dynamic_sock);
            continue;
        }

        // Asignamos el socket a la dirección y puerto especificados
        if (bind(dynamic_sock, (struct sockaddr*)&dynamic_addr, sizeof(dynamic_addr)) < 0) {
            perror("Bind error on dynamic port");
            close(dynamic_sock);
            continue;
        }
        
        // Escuchamos conexiones entrantes
        if (listen(dynamic_sock, 1) < 0) {
            perror("Listen error on dynamic port");
            close(dynamic_sock);
            continue;
        }
        
        printf("[*] Assigned dynamic port %d to client\n", dynamic_port);

        // Aceptamos la conexión del cliente en el puerto dinámico
        int dynamic_client = accept(dynamic_sock, NULL, NULL);
        if (dynamic_client < 0) {
            perror("Accept error on dynamic port");
            close(dynamic_sock);
            continue;
        }

        int* sockets = malloc(2 * sizeof(int));
        sockets[0] = dynamic_client;
        sockets[1] = dynamic_sock;
        
        pthread_t handler_thread;
        pthread_create(&handler_thread, NULL, connectionHand, sockets);
        pthread_detach(handler_thread);
        usleep(1000); 
    }
    
    close(port_s);
    return 0;
}
