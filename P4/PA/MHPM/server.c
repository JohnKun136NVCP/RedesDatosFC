#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <time.h>

#define BUFFER_SIZE 2048
#define PORT 49200

// Función para obtener la fecha y hora 
void get_timestamp(char *timestamp) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(timestamp, 20, "%Y-%m-%d %H:%M:%S", t);
}

// Función que maneja cada servidor en un alias específico
void run_server(char *alias) {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    char buffer[BUFFER_SIZE];
    char timestamp[20];

    // hacemos un socket TCP
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("[-] socket");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Permitir reuso de dirección, es para poder reutizar la dirección justo después de cerrar 
    // la terminal 
    int opt = 1;
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("[-] setsockopt");
        close(server_sock);
        exit(1);
    }

    // Asocia el socket con la dirección y puerto que definimos 
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] bind");
        close(server_sock);
        exit(1);
    }

    // solo podemos tener 5 conexiones por mucho
    if (listen(server_sock, 5) < 0) {
        perror("[-] listen");
        close(server_sock);
        exit(1);
    }

    printf("[Servidor %s] Escuchando en puerto %d (PID: %d)\n", alias, PORT, getpid());

    //Acepta las conexiones entrantes 
    while (1) {
        addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
        if (client_sock < 0) {
            perror("[-] accept");
            continue;
        }

        // recibe los datos del cliente
        int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            close(client_sock);
            continue;
        }
        buffer[bytes] = '\0';

        get_timestamp(timestamp);
        printf("[Servidor %s] %s - Cliente conectado: %s\n", 
               alias, timestamp, inet_ntoa(client_addr.sin_addr));

        // Envia confirmación de recepción
        char response[BUFFER_SIZE];
        snprintf(response, sizeof(response), "OK:%s:%s", alias, timestamp);
        send(client_sock, response, strlen(response), 0);

        close(client_sock);
    }

    close(server_sock);
}

// verificamos que se haya proporcionadoun alias 
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Uso: %s <alias1> <alias2> ...\n", argv[0]);
        return 1;
    }

    int num_aliases = argc - 1;
    pid_t pids[num_aliases];

    printf("[Servidor Principal] Iniciando servidores en aliases...\n");

    // Crear procesos hijos para cada alias
    for (int i = 0; i < num_aliases; i++) {
        pids[i] = fork();
        
        if (pids[i] == 0) { // Proceso hijo
            run_server(argv[i + 1]);
            exit(0);
        } else if (pids[i] < 0) { // Error al crear proceso
            perror("[-] fork");
            exit(1);
        }
    }
    
    printf("[Servidor Principal] Todos los servidores iniciados. PIDs:\n");
    for (int i = 0; i < num_aliases; i++) {
        printf(" - Alias %s: PID %d\n", argv[i + 1], pids[i]);
    }

    // Espera a que todos los procesos hijos terminen
    for (int i = 0; i < num_aliases; i++) {
        waitpid(pids[i], NULL, 0);
    }

    return 0;
}