#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <pthread.h> // para HILOS

#define PUERTO 49200
#define PUERTO_COORD 49300  // para comuniacion entre servidores
#define BUFFER_SIZE 1024

// VARIABLES GLOBALES (compartidas entre hilos)
char servidor_estado[20] = "esperando";

int server_sock;  // socket principal
int coord_sock;   // socket de comunicacion/coordinacion
pthread_mutex_t estado_mutex;  // exlusion mutua
int turno_actual = 0;  // 0=s01, 1=s02, 2=s03, 3=s04
int mi_id = 0;  // ID de este servidor
char mi_directorio[20] = "s01"; // donde se guardan los archivos por servidor
 
void* aceptar_clientes(void* arg) {
    struct sockaddr_in client_addr;
    socklen_t addr_size = sizeof(client_addr);
    
    printf("[=] Hilo 1 - Esperando clientes...\n");
    
    while(1) {
        int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
        if(client_sock < 0){
            perror("[-] Error en accept");
            continue;
        }
        
        printf("[+] Cliente conectado\n");
        
        // Cambiar estado
        pthread_mutex_lock(&estado_mutex);
        strcpy(servidor_estado, "recibiendo");
        pthread_mutex_unlock(&estado_mutex);
        
        // Recibir mensaje
        char buffer[BUFFER_SIZE] = {0};
        int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes > 0) {
            buffer[bytes] = '\0';
            printf("[+] Mensaje recibido: %s\n", buffer);
            
            // Guardar mensaje
            char filepath[512];
            snprintf(filepath, sizeof(filepath), "%s/mensajes.txt", mi_directorio);
            FILE *fp = fopen(filepath, "a");
            if (fp != NULL) {
                fprintf(fp, "%s\n", buffer);
                fclose(fp);
                send(client_sock, "OK", 2, 0);
            } else {
                send(client_sock, "ERROR", 5, 0);
            }
        }
        
        close(client_sock);
        
        // Volver a estado esperando
        pthread_mutex_lock(&estado_mutex);
        strcpy(servidor_estado, "esperando");
        pthread_mutex_unlock(&estado_mutex);
    }
    return NULL;
}

void* escuchar_servidores(void* arg) {

    printf("[=] Hilo 2- Escuchando servidores...\n");
   
    coord_sock = socket(AF_INET, SOCK_DGRAM, 0);  // UDP 
    if(coord_sock == -1) {
        perror("[-] Error creando el socket de coordinación");
        return NULL;
    }
    
    struct sockaddr_in coord_addr;
    coord_addr.sin_family = AF_INET;
    coord_addr.sin_port = htons(PUERTO_COORD);
    coord_addr.sin_addr.s_addr = INADDR_ANY;
    
    if(bind(coord_sock, (struct sockaddr*)&coord_addr, sizeof(coord_addr)) < 0) {
        perror("[-] Error haciendo bien en el socket de coordinación");
        return NULL;
    }
    
    while(1) {
        char buffer[BUFFER_SIZE];
        struct sockaddr_in sender_addr;
        socklen_t addr_len = sizeof(sender_addr);
        
        int bytes = recvfrom(coord_sock, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&sender_addr, &addr_len);
        
        if (bytes > 0) {
            buffer[bytes] = '\0';
            printf("[+] Mensaje de otro servidor: %s\n", buffer);
            
            // procesar mensaje (ej: "s02:recibiendo")
            pthread_mutex_lock(&estado_mutex);
            // se actualiza el estado de acuerdo al mensaje
            pthread_mutex_unlock(&estado_mutex);
        }
    }
    return NULL;
}

void* gestionar_turnos(void* arg) {
    printf("[=] Hilo 3 - Gestion de turnos...\n");

    int quantum = 5;  // 5 seg por turno
    
    while(1) {
        pthread_mutex_lock(&estado_mutex);
        
        if(turno_actual == mi_id) {
            printf("[TURNO] Es el turno de servidor %d\n", mi_id);
            
            char msg[50]; // mensaje de notificacion
            snprintf(msg, sizeof(msg), "TURNO:%d", mi_id);
            
            // broadcast (se transmite el turno de un servidor)
            for (int i = 0; i < 4; i++) {
                if (i != mi_id) {
                    struct sockaddr_in dest_addr;
                    dest_addr.sin_family = AF_INET;
                    dest_addr.sin_port = htons(PUERTO_COORD + i);
                    dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // localhost
                    
                    sendto(coord_sock, msg, strlen(msg), 0, 
                          (struct sockaddr*)&dest_addr, sizeof(dest_addr));
                }
            }
            
        }else {
            printf("Turno de servidor %d (esperando...)\n", turno_actual);
            strcpy(servidor_estado, "esperando_turno");
        }
        
        pthread_mutex_unlock(&estado_mutex);
        
        sleep(quantum);
        
        // se sigue al siguiente turno
        pthread_mutex_lock(&estado_mutex);
        turno_actual = (turno_actual + 1) % 4;  // -> 4 servidores (s01, s02, s03, s04)
        pthread_mutex_unlock(&estado_mutex);
    }
    return NULL;
}


char* identificar_alias_cliente(struct sockaddr_in client_addr) {    
    // IMPORTANTE: tuve que cambiar las alias IPs 92.168.0.10X a 92.168.0.10X
    // porque no me hacia el ping con la que tiene 0
    char* client_ip = inet_ntoa(client_addr.sin_addr);
    
    if(strcmp(client_ip, "192.168.1.101") == 0){
        return "s01";
    } else if (strcmp(client_ip, "192.168.1.102") == 0) {
        return "s02";
    } else if (strcmp(client_ip, "192.168.1.103") == 0) {  // ← FALTABA
        return "s03";
    } else if (strcmp(client_ip, "192.168.1.104") == 0) {  // ← FALTABA
        return "s04";
    } else if (strcmp(client_ip, "127.0.0.1") == 0) {
        return "localhost";
    } else {
        return "desconocido";
    }
}

void obtener_info_cliente(int client_sock, char* client_info) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    // https://pubs.opengroup.org/onlinepubs/007904875/functions/getpeername.html
    
    if (getpeername(client_sock, (struct sockaddr*)&client_addr, &addr_len) == 0) {
        char* client_ip = inet_ntoa(client_addr.sin_addr);
        int client_port = ntohs(client_addr.sin_port);
        char* alias = identificar_alias_cliente(client_addr);
        
        snprintf(client_info, BUFFER_SIZE, "IP: %s, Puerto: %d, Alias: %s", 
                 client_ip, client_port, alias);
    } else {
        strcpy(client_info, "Cliente desconocido");
    }
}


int main(int argc, char *argv[]) {
    char mi_alias[10] = "s01";  // servidor por defecto
    
    if(argc > 1) {
        strcpy(mi_alias, argv[1]);
        // los alias estas asociados a un id -> ej s01 -> 0, s02 -> 1, ...
        if(strlen(argv[1]) >= 3 && argv[1][0] == 's' && argv[1][1] == '0') {
            mi_id = argv[1][2] - '1'; 
        }
    }
    
    printf("Iniciando servidor %s (ID: %d)\n", mi_alias, mi_id);
    
    // INICIAR MUTEX
    pthread_mutex_init(&estado_mutex, NULL);
    
    // socket principal
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("[-] Error creando socket");
        return 1;
    }
    
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    // SO_REUSEADDR --> permite que un socket se vincule forzosamente a un puerto en uso por otro socket

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PUERTO);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    if(bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] Error en bind");
        close(server_sock);
        return 1;
    }
    
    if(listen(server_sock, 5) < 0) {
        perror("[-] Error en listen");
        close(server_sock);
        return 1;
    }
    
    printf("[+] Servidor %s escuchando en puerto %d\n", mi_alias, PUERTO);
    
    // iniciar hilos
    pthread_t hilo1, hilo2, hilo3;
    pthread_create(&hilo1, NULL, aceptar_clientes, NULL);
    pthread_create(&hilo2, NULL, escuchar_servidores, NULL);
    pthread_create(&hilo3, NULL, gestionar_turnos, NULL);
    
    // esperar hilos
    pthread_join(hilo1, NULL);
    pthread_join(hilo2, NULL);
    pthread_join(hilo3, NULL);
    
    pthread_mutex_destroy(&estado_mutex);
    close(server_sock);
    return 0;
}