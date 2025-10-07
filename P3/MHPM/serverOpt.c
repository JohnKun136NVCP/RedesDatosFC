#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>

//aumentamos el tamaño del buffer debido a que el tamaño de la caden atambién aumentó 
#define BUFFER_SIZE 2048
#define NUM_PORTS 3 //el número de puertos que vamos a tener

// Función de cifrado César, reutilizamos la misma de la práctica 2
void encryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            text[i] = ((c - 'A' + shift) % 26) + 'A';
        } else if (islower(c)) {
            text[i] = ((c - 'a' + shift) % 26) + 'a';
        } else {
            text[i] = c;
        }
    }
}

// Función que maneja cada servidor en un puerto específico
void run_server(int port) {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    char buffer[BUFFER_SIZE];

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("[-] socket");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Permitir reuso de dirección
    int opt = 1;
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("[-] setsockopt");
        close(server_sock);
        exit(1);
    }

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] bind");
        close(server_sock);
        exit(1);
    }

    if (listen(server_sock, 5) < 0) {
        perror("[-] listen");
        close(server_sock);
        exit(1);
    }
    //muestra el PID que se crea por el fork()
    printf("[Servidor] Escuchando en puerto %d (PID: %d)\n", port, getpid());

    while (1) {
        addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
        if (client_sock < 0) {
            perror("[-] accept");
            continue;
        }

        int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            close(client_sock);
            continue;
        }
        buffer[bytes] = '\0';

        // Definnimos los argumentos que va recibir (puerto, desplazamiento y mensaje)
        int puerto_objetivo, shift;
        char texto[BUFFER_SIZE];
        sscanf(buffer, "%d %d %[^\n]", &puerto_objetivo, &shift, texto);

        if (puerto_objetivo == port) {
            encryptCaesar(texto, shift);
            char response[BUFFER_SIZE];
            // Indica en qué puerto se está llevando a cabo la encriptación
            snprintf(response, sizeof(response), "Procesado en puerto %d", port);
            send(client_sock, response, strlen(response), 0);
            // Mostrar en consola solo información del procesamiento
            printf("[Servidor %d] Texto procesado (longitud: %zu caracteres)\n", port, strlen(texto));
        } else {
            char response[BUFFER_SIZE];
            // Indica en qué puertos fueron rechazados 
            snprintf(response, sizeof(response), "Rechazado en puerto %d", port);
            send(client_sock, response, strlen(response), 0);
            printf("[Servidor %d] Solicitud rechazada\n", port);
        }

        close(client_sock);
    }

    close(server_sock);
}

int main() {
    int ports[NUM_PORTS] = {49200, 49201, 49202}; //definimos los tres puertos que podemos tener 
    pid_t pids[NUM_PORTS];

    printf("[Servidor Principal] Iniciando servidores en múltiples puertos...\n");

    /**
     * Justo en esta parte es donde creamos lo dicho en el documento, es decir, usar el fork() para crear un 
     * puerto hijo 
     */

    // Crear procesos hijos para cada puerto
    for (int i = 0; i < NUM_PORTS; i++) {
        pids[i] = fork();
        
        if (pids[i] == 0) { // Proceso hijo
            run_server(ports[i]);
            exit(0);
        } else if (pids[i] < 0) { // Error al crear proceso
            perror("[-] fork");
            exit(1);
        }
    }
    
    // Se asigan un identificador de proceso (PID) 
    printf("[Servidor Principal] Todos los servidores iniciados. PIDs:\n");
    for (int i = 0; i < NUM_PORTS; i++) {
        printf(" - Puerto %d: PID %d\n", ports[i], pids[i]);
    }

    /**
     * Esperar a que todos los procesos hijos terminen
     *Una vez que termine al volver a ejecutar ./serverOpt estos saldrán como "en uso", debido a que ya fueron
     * ocupados por un mensaje, en caso de volver a querer a usarlos se tiene que cerrar la terminal y abrir
     * una nueva 
     */
    for (int i = 0; i < NUM_PORTS; i++) {
        waitpid(pids[i], NULL, 0);
    }

    return 0;
}