#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define NUM_SERVERS 3
#define TURN_TIMEOUT 10  // Tiempo por turno 

// Variables globales para control de turnos
pthread_mutex_t turn_mutex = PTHREAD_MUTEX_INITIALIZER; // semaforo para turnos
pthread_cond_t turn_cond = PTHREAD_COND_INITIALIZER;
int current_turn = 0;  // servidor en turno (0, 1, 2)
int server_busy = 0;   // indica si un servidor está recibiendo

// Estructura para pasar datos a los hilos
typedef struct {
    int port;
    int server_id;
} ServerData;

// Función para descifrar el texto usando el cifrado César
void decryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            text[i] = ((c - 'A' - shift + 26) % 26) + 'A';
        } else if (islower(c)) {
            text[i] = ((c - 'a' - shift + 26) % 26) + 'a';
        } else {
            text[i] = c;
        }
    }
}

int wait_for_turn(int server_id) {
    pthread_mutex_lock(&turn_mutex); // bloquear mutex
    
    // Esperar hasta que sea nuestro turno
    while (current_turn != server_id) {
        pthread_cond_wait(&turn_cond, &turn_mutex);
    }
    
    printf("[+][Server %d] Es mi turno (Puerto %d)\n", 
           server_id, 49200 + server_id);
    
    pthread_mutex_unlock(&turn_mutex);
    return 1;
}

// Función para liberar turno y pasar al siguiente servidor
void release_turn(int server_id) {
    pthread_mutex_lock(&turn_mutex);
    
    printf("[+][Server %d] Liberando turno\n", server_id);
    
    // Pasar al siguiente servidor
    current_turn = (current_turn + 1) % NUM_SERVERS;
    server_busy = 0;
    
    // Notificar a todos los hilos que el turno ha cambiado
    pthread_cond_broadcast(&turn_cond);
    
    pthread_mutex_unlock(&turn_mutex);
}

//  marcar si el servidor está recibiendo
void mark_busy(int server_id) {
    pthread_mutex_lock(&turn_mutex);
    server_busy = 1;
    printf("[+][Server %d] Notificando a otros servidores: RECIBIENDO...\n", server_id);
    pthread_mutex_unlock(&turn_mutex);
}

//  verificar si hay conexión pendiente (no bloqueante)
int check_pending_connection(int listen_sock) {
    fd_set readfds;
    struct timeval tv;
    
    FD_ZERO(&readfds);
    FD_SET(listen_sock, &readfds);
    
    // timeout de 1 segundo
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    
    int result = select(listen_sock + 1, &readfds, NULL, NULL, &tv);
    
    return (result > 0 && FD_ISSET(listen_sock, &readfds));
}

void* server_thread(void* arg) {
    ServerData *data = (ServerData*)arg;
    int port = data->port;
    int server_id = data->server_id;
    
    // Crear socket de escucha
    int listen_sock;
    struct sockaddr_in addr;
    
    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == -1) {
        perror("[-] Error to create the socket");
        pthread_exit(NULL);
    }
    
    // Permitir reutilización de dirección
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[-] Error binding");
        close(listen_sock);
        pthread_exit(NULL);
    }
    
    if (listen(listen_sock, 5) < 0) {
        perror("[-] Error on listen");
        close(listen_sock);
        pthread_exit(NULL);
    }
    
    printf("[+] Server %d listening on port %d...\n", server_id, port);
    
    while (1) {
        // Esperar nuestro turno
        wait_for_turn(server_id);
        
        time_t turn_start = time(NULL);
        int received_file = 0;
        
        printf("[+][Server %d] Verificando conexiones pendientes...\n", server_id);
        
        while (difftime(time(NULL), turn_start) < TURN_TIMEOUT) {
            if (check_pending_connection(listen_sock)) {
                struct sockaddr_in client_addr;
                socklen_t addr_size = sizeof(client_addr);
                int client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &addr_size);
                
                if (client_sock < 0) {
                    perror("[-] Error on accept");
                    continue;
                }
                
                // Marcar que estamos ocupados
                mark_busy(server_id);
                received_file = 1;
                
                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
                int client_port = ntohs(client_addr.sin_port);
                
                printf("[+][Server %d] Cliente conectado desde %s:%d\n", 
                       server_id, client_ip, client_port);
                
                char buffer[BUFFER_SIZE * 2] = {0};
                char mensaje_cifrado[BUFFER_SIZE] = {0};
                int shift = 0;
                
                // Recibir el archivo completo sin importar el tiempo
                int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
                if (bytes <= 0) {
                    printf("[-][Server %d] No se recibió mensaje del cliente\n", server_id);
                    close(client_sock);
                    break;
                }
                
                buffer[bytes] = '\0';
                printf("[+][Server %d] Datos recibidos: %s\n", server_id, buffer);
                
                // Separar el mensaje cifrado del shift
                char *token = strtok(buffer, "|");
                if (token != NULL) {
                    strcpy(mensaje_cifrado, token);
                    token = strtok(NULL, "|");
                    if (token != NULL) {
                        shift = atoi(token);
                    } else {
                        printf("[-][Server %d] Error: Valor de shift no encontrado\n", server_id);
                        send(client_sock, "REJECTED - Invalid format", 
                             strlen("REJECTED - Invalid format"), 0);
                        close(client_sock);
                        break;
                    }
                } else {
                    printf("[-][Server %d] Error: Formato de mensaje inválido\n", server_id);
                    send(client_sock, "REJECTED - Invalid format", 
                         strlen("REJECTED - Invalid format"), 0);
                    close(client_sock);
                    break;
                }
                
                printf("[+][Server %d] Mensaje cifrado: %s\n", server_id, mensaje_cifrado);
                printf("[+][Server %d] Valor de shift: %d\n", server_id, shift);
                
                // Descifrar el mensaje
                decryptCaesar(mensaje_cifrado, shift);
                printf("[+][Server %d] Mensaje descifrado: %s\n", server_id, mensaje_cifrado);
                
                // Enviar confirmación al cliente
                char acceptance_msg[100];
                snprintf(acceptance_msg, sizeof(acceptance_msg), 
                         "SERVER RESPONSE %d: File received and decrypted.", port);
                send(client_sock, acceptance_msg, strlen(acceptance_msg), 0);
                
                printf("[+][Server %d] ACCEPTED - Archivo recibido completamente\n", server_id);
                close(client_sock);
                printf("[+][Server %d] Cliente desconectado\n\n", server_id);
                
                // Salir del bucle después de recibir el archivo
                break;
            }
            
            // Pequeña pausa para no consumir CPU
            usleep(100000);  // 100ms
        }
        
        if (!received_file) {
            printf("[+][Server %d] No hay archivos por recibir, dejando turno\n", server_id);
        }
        
        // Liberar turno para el siguiente servidor
        release_turn(server_id);
        
        // Pequeña pausa antes de volver a esperar turno
        usleep(100000);
    }
    
    close(listen_sock);
    pthread_exit(NULL);
}

int main() {
    int ports[NUM_SERVERS] = {49200, 49201, 49202};
    pthread_t threads[NUM_SERVERS];
    ServerData server_data[NUM_SERVERS];
    
    printf("[+] Iniciando servidor multi-puerto con Round Robin\n");
    printf("[+] Puertos: %d, %d, %d\n", ports[0], ports[1], ports[2]);
    printf("[+] Tiempo por turno: %d segundos\n\n", TURN_TIMEOUT);
    
    // Crear un hilo para cada servidor
    for (int i = 0; i < NUM_SERVERS; i++) {
        server_data[i].port = ports[i];
        server_data[i].server_id = i;
        
        if (pthread_create(&threads[i], NULL, server_thread, &server_data[i]) != 0) {
            perror("[-] Error creating thread");
            exit(1);
        }
    }
    
    // Esperar a que todos los hilos terminen (nunca debería pasar)
    for (int i = 0; i < NUM_SERVERS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    pthread_mutex_destroy(&turn_mutex);
    pthread_cond_destroy(&turn_cond);
    
    return 0;
}